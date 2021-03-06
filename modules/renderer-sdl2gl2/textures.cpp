#include <cstring>
#include <cstdio>
#include <string>

#include "utlist.h"

#include "SDL.h"
#include "SDL_pnglite.h"

#include "iplatform.h"

#define DFMODULE_BUILD
#include "itextures.h"

/*
    This is a GL-oriented SDL texture album, optimized for loading speed.
    Thus, convert_magenta, clone and grayscale operations are not done,
    only recorded in the index, assumed to be offloaded to the pixel shader.
    The album is also not shrunk upon filling.
*/

extern iplatform *platform;
extern getplatform_t _getplatform;

namespace {

ilogger *logr, *delogr, *dumplogr;

struct celpage_t {
    int w, h, cw, ch;
    SDL_Surface *s;
    bool magentic;
    int start_index;
    int refcount; // how many clones does it have
    char *sourcefile;

    struct celpage_t *prev;
    struct celpage_t *next;
};

struct celclone_t {
    long index;   // this cel index
    long source;  // source cel index
    bool gray;    // was it grayed or just cloned

    struct celclone_t *prev;
    struct celclone_t *next;
};

struct implementation : public itextures {
    void release();

    long clone_texture(long src);
    void grayscale_texture(long pos);
    void load_multi_pdim(const char *filename, long *tex_pos, long dimx, long dimy,
                                   bool convert_magenta, long *disp_x, long *disp_y);
    long load(const char *filename, bool convert_magenta);
    void delete_texture(long pos);

    df_texalbum_t *get_album();
    void release_album(df_texalbum_t *);

    void set_rcfont(const void *, int);
    void reset();

    /* --- */

    long find_next_index();
    void insert_page(celpage_t *);
    celpage_t * find_page(long);
    long next_index; // == .size(); next unused index.
    implementation() :
        next_index(0),
        pages(NULL),
        clones(NULL),
        w_lcm(1),
        rc_font_set(false) { }

    celpage_t *pages;    // kept sorted in descending cel height
    celclone_t *clones;
    int w_lcm;
    bool rc_font_set;
};

/* pieces of fgt.gl.rgba_surface */
SDL_Surface *beloved_surface(int w, int h) {
    int bpp;
    Uint32 Rmask, Bmask, Amask, Gmask;

    SDL_PixelFormatEnumToMasks(SDL_PIXELFORMAT_ABGR8888,
        &bpp, &Rmask, &Gmask, &Bmask, &Amask);

    SDL_Surface *s = SDL_CreateRGBSurface(0, w, h,
        bpp, Rmask, Gmask, Bmask, Amask);

    SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_NONE);

    return s;
}

/* make sure it's of our belowed pixelformat and blendmode is none */
SDL_Surface *load_file(const char *filename) {
    SDL_Surface *s = SDL_LoadPNG(filename);
    if (!s) {
        logr->info("SDL_LoadPNG(): %s", SDL_GetError());
        s = SDL_LoadBMP(filename);
    }
    if (!s) {
        logr->info("SDL_LoadBMP(): %s", SDL_GetError());
        return NULL;
    }
    return s;
}

SDL_Surface *normalize(SDL_Surface *s) {
    SDL_Surface *d = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0);

    if (!d) {
        logr->error("SDL_ConvertSurfaceFormat() failed : %s", SDL_GetError());
        return NULL;
    }

    SDL_SetSurfaceBlendMode(d, SDL_BLENDMODE_NONE);
    return d;
}

SDL_Surface *load_normalize(const char *filename) {
    SDL_Surface *s = load_file(filename);
    if (s) {
        SDL_Surface *d = normalize(s);
        SDL_FreeSurface(s);
        s = d;
    }
    return s;
}

int gcd(int n, int m) { return m == 0 ? n: gcd(m, n%m); }
int lcm(int a, int b) { return a * b / gcd(a, b); }

SDL_Surface *grow_album(SDL_Surface *album, int min_h, bool fill = false) {
    if (min_h > album->h) { // grow it
        SDL_Surface *a = beloved_surface(album->w, album->h*2);

        if (fill) { // hmm. can't hurt, but at the same time, no one cares.
            SDL_Rect unused = { 0, album->h, album->w, album->h };
            SDL_FillRect(a, &unused, 0);
        }

        SDL_Rect used = { 0, 0, album->w, album->h };
        SDL_BlitSurface(album, &used, a, &used);

        SDL_Surface *tmp = album;
        album = a;
        SDL_FreeSurface(tmp);
    }
    return album;
}

void dump_album(df_texalbum_t *ta, const char *name) {
    std::string fn(name);
    fn += ".png";
    SDL_SavePNG(ta->album, fn.c_str());
    fn = name;
    fn += ".index";
    FILE *dump = fopen(fn.c_str(), "w");
    if (!dump)
        logr->fatal("can't dump to '%s'",fn.c_str());

    fprintf(dump, "count: %d\nheight=%d\n", ta->count, ta->height);
    for (unsigned i=0; i < ta->count ; i++ ) {
        df_taindex_entry_t& e = ta->index[i];
        fprintf(dump, "%d %dx%d+%d+%d [%c%c]\n",
            i, e.rect.w, e.rect.h, e.rect.x, e.rect.y,
            e.magentic ? 'm' : ' ', e.gray ? 'g' : ' ');
    }
    fclose(dump);
}

/* see fgtestbed fgt.raw.Pageman

   This gets called from renderer thread while the load_* stuff
   gets called from simu thread. There is no sync atm, just hope it
   doesn't crash and burn for now.
*/
df_texalbum_t *implementation::get_album() {
    int album_w = 1024; // it's pot; and dumps are manageable in a viewer.

    df_texalbum_t *rv = (df_texalbum_t *) calloc(1, sizeof(df_texalbum_t));
    rv->count = next_index;
    rv->index = (df_taindex_entry_t *) calloc(rv->count, sizeof(df_taindex_entry_t));

    rv->album = beloved_surface(album_w, album_w / 8);

    album_w -= album_w % w_lcm; // actual surface width stays power-of-two.

    int cx = 0, cy = 0, row_h = pages->ch;

    celpage_t *page;
    DL_FOREACH(pages, page) {
        unsigned index = page->start_index;
        /*  When to start new rows - when this minimizes wasted space.

            waste_start_new_row = (album_w - cx) * old_row_h

            if new_row_h > old_row_h:
                waste_just_modify_row_h = cx * (new_row_h - old_row_h)
                requires changing row_h now
            else:
                waste_just_modify_row_h = (album_w - cx) * (old_row_h - new_row_h)
                requires not changing row_h until next row

            Since we sort the pages before processing them, it'll be
            either p->ch > row_h or otherwise for the whole album, never both.
            Otherwise we'd have to store the history of row_h changes over a row,
            and wasted space calculation will be complex.

            Both branches are nevertheless left here in case sort order will change.

            Wrapping this in an optimizing algorithm that would select optimal album_w,
            or implementing a tight packer that would achieve minimal waste while relaxing
            restriction that cels from one cel page always follow each other ..
                --  is very not worth it. */

        int wasted = (album_w - cx) * row_h; // when starting new row
        if (page->ch > row_h) {
            if ( wasted < (page->ch - row_h) * cx ) {
                cx = 0;
                cy += row_h;
            }
            row_h = page->ch;
        } else {
            if ( wasted < (album_w - cx) * (row_h - page->ch) ) {
                cx = 0;
                cy += row_h;
                row_h = page->ch;
            }
        }

        /*  This can produce a false positive if p->ch < row_h
            and we decided to not start new row. */
        rv->album = grow_album(rv->album, cy + row_h);

        for (int j = 0; j < page->h; j++) {
            for (int i = 0; i < page->w; i++) {
                /* Another possible optimization would be to blit entire series
                   of horizontally-adjacent cels when they fit into the current
                   album row. Don't think it is worth the code complexity though. */
                if (cx >= album_w) { /* begin new row. */
                    cx = 0;
                    cy += row_h;   /* grow by old row_h which might be > than current page's ch */
                    row_h = page->ch; /* thus set row_h in case we did not at the start of the page */
                    rv->album = grow_album(rv->album, cy + row_h);
                }
                SDL_Rect src = { i*page->cw, j*page->ch, page->cw, page->ch };
                rv->index[index].rect.x = cx;
                rv->index[index].rect.y = cy;
                rv->index[index].rect.w = page->cw;
                rv->index[index].rect.h = page->ch;
                SDL_BlitSurface(page->s, &src, rv->album, &rv->index[index].rect);
                rv->index[index].magentic = page->magentic;
                cx += page->cw;
                index ++;
            }
        }
    }

    /* do this instead of shrink */
    rv->height = cy + row_h;

    /* execute clone requests */
    celclone_t *item;
    DL_FOREACH(clones, item) {
        rv->index[item->index] = rv->index[item->source]; // copy?
        rv->index[item->index].gray = item->gray;
    }
#if defined(TEXALBUM_DUMP)
    dump_album(rv, "texalbum");
#endif
    return rv;
}

void implementation::release_album(df_texalbum_t *a) {
    SDL_FreeSurface(a->album);
    free(a->index);
    free(a);
}

celpage_t * implementation::find_page(long pos) {
    celpage_t *page;
    DL_FOREACH(pages, page) {
        long page_last_index = page->start_index + page->w * page->h - 1;
        if ((page->start_index <= pos) && (page_last_index >= pos)) {
            return page;
        }
    }
    return NULL;
}

long implementation::clone_texture(long src) {
    celclone_t *item = (celclone_t *)malloc(sizeof(celclone_t));
    item->index = next_index;
    item->source = src;
    item->gray = false;
    DL_PREPEND(clones, item);

    celpage_t *page = find_page(src);
    if (page)
        page->refcount += 1;

    return next_index++;
}

void implementation::grayscale_texture(long pos) {
    /* first see if it was cloned (it has to be) */
    celclone_t *item;
    DL_FOREACH(clones, item) {
        if (item->index == pos) {
            item->gray = true;
            return;
        }
    }
    /* handle this later. */
    logr->fatal("graying non-cloned texture %ld", pos);
}

void implementation::insert_page(celpage_t *item) {
    /* to keep the list sorted, insert new pages before any with lower ch or at the end. */

    celpage_t *larger_h_page = NULL, *page;
    DL_FOREACH(pages, page) {
        if (page->ch < item->ch) {
            larger_h_page = page;
            break;
        }
    }
    if (larger_h_page)
        DL_PREPEND_ELEM(pages, larger_h_page, item);
    else
        DL_APPEND(pages, item);

    /* also keep w_lcm updated. */
    w_lcm = lcm(w_lcm, item->cw);
}
void implementation::load_multi_pdim(const char *filename, long *tex_pos,
      long dimx, long dimy, bool convert_magenta, long *disp_x, long *disp_y) {
    logr->trace("load_multi_pdim(%s, %ld, %ld, %d)", filename, dimx, dimy, (int)convert_magenta);
    if (rc_font_set)
        reset();

    SDL_Surface *s = load_normalize(filename);
    if (!s)
        logr->fatal("failed to load %s", filename);

    *disp_x = s->w/dimx, *disp_y = s->h/dimy;

    celpage_t *item = (celpage_t*)malloc(sizeof(celpage_t));
    item->sourcefile = (char *)malloc(strlen(filename) + 1);
    strcpy(item->sourcefile, filename);
    item->w = dimx; item->h = dimy; item->cw = *disp_x; item->ch = *disp_y;
    item->s = s; item->magentic = convert_magenta; item->start_index = next_index;
    item->refcount = dimx*dimy;

    insert_page(item);

    for (int i = 0; i < dimx*dimy; i++)
        tex_pos[i] = next_index++;
}

long implementation::load(const char *filename, bool convert_magenta) {
    logr->trace("load(%s), %d", filename, (int)convert_magenta);
    if (rc_font_set)
        reset();

    SDL_Surface *s = load_normalize(filename);
    if (!s)
        logr->fatal("failed to load %s", filename);

    celpage_t *item = (celpage_t*)malloc(sizeof(celpage_t));
    item->sourcefile = (char *)malloc(strlen(filename) + 1);
    strcpy(item->sourcefile, filename);
    item->w = 1; item->h = 1; item->cw = s->w; item->ch = s->h;
    item->s = s; item->magentic = convert_magenta; item->start_index = next_index;
    item->refcount = 1;
    insert_page(item);

    return next_index++;
}

void implementation::set_rcfont(const void *ptr, int len) {
    SDL_RWops *rw = SDL_RWFromConstMem(ptr, len);
    if (!rw)
        logr->fatal("SDL_RWFromConstMem(): %s", SDL_GetError());

    SDL_Surface *s = SDL_LoadPNG_RW(rw, 1);

    if (!s)
        logr->fatal("SDL_LoadPNG_RW(): %s", SDL_GetError());

    SDL_Surface *d = normalize(s);
    SDL_FreeSurface(s);
    s = d;

    if (next_index)
        reset();

    celpage_t *item = (celpage_t*)malloc(sizeof(celpage_t));
    item->w = 16; item->h = 16; item->cw = s->w/16; item->ch = s->h/16;
    item->s = s; item->magentic = true; item->start_index = 0;
    item->refcount = 0; // not referenced by anything in DF binary
    DL_PREPEND(pages, item);

    rc_font_set = true;
    next_index = 256;
    logr->trace("set_rcfont(): %dx%d", s->w/16, s->h/16);
}

void implementation::reset() {
    rc_font_set = false;
    next_index = 0;
    w_lcm = 1;

    celpage_t *page, *ptmp;
    DL_FOREACH_SAFE(pages, page, ptmp) {
        DL_DELETE(pages, page);
        SDL_FreeSurface(page->s);
        free(page->sourcefile);
        free(page);
    }

    celclone_t *item, *ctmp;
    DL_FOREACH_SAFE(clones, item, ctmp) {
        DL_DELETE(clones, item);
        free(item);
    }

    logr->trace("reset()");
}

long implementation::find_next_index() {
    long rv = 0;
    celpage_t *page;
    DL_FOREACH(pages, page) {
        long page_last_index = page->start_index + page->w * page->h - 1;
        if (page_last_index > rv)
            rv = page_last_index;
    }
    celclone_t *item;
    DL_FOREACH(clones, item) {
        if (item->index > rv)
            rv = item->index;
    }
    return rv + 1;
}
void implementation::delete_texture(long pos) {
    /* stark raving inefficient */
    celclone_t *item, *tmp;
    celpage_t *page = NULL;
    DL_FOREACH_SAFE(clones, item, tmp) {
        if (item->index == pos) {
            DL_DELETE(clones, item);
            page = find_page(item->source);
            if (!page)
                delogr->fatal("No source page for clone %ld", pos);
            page->refcount -= 1;
            free(item);
            next_index = find_next_index();
            delogr->trace("delete_texture(%ld): done, page %s refcount=%d", pos, page->sourcefile, page->refcount);
            break;
        }
    }
    if (!page) {
        /* it's not a clone */
        page = find_page(pos);
        if (!page) {
            delogr->warn("delete_texture(%ld): not found at all.");
            return;
        }
        page->refcount -= 1;
    }

    if (page->refcount == 0) {
        DL_DELETE(pages, page);
        if (delogr->enabled(LL_TRACE)) {
            int c_count, p_count;
            DL_COUNT(clones, c_count);
            DL_COUNT(pages, p_count);
            delogr->info("delete_texture(%ld): dropped page %s, %d pages %d clones left",
                pos, page->sourcefile, p_count, c_count);
        }
        SDL_FreeSurface(page->s);
        free(page->sourcefile);
        free(page);
    }
}
void implementation::release() { }

static implementation *impl = NULL;

extern "C" DFM_EXPORT itextures * DFM_APIEP gettextures(void) {
    if (!impl) {
        platform = _getplatform();
        logr = platform->getlogr("sdl.textures");
        delogr = platform->getlogr("sdl.textures.delete");
        dumplogr = platform->getlogr("sdl.textures.dump");
        impl = new implementation();
    }
    return impl;
}
} /* ns */
