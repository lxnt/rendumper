#include <cstring>
#include <string>
#include <fstream>
#include <forward_list>

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

ilogger *logr;

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

    long next_index; // == .size(); next unused index.
    implementation() :
        next_index(0),
        pages(),
        clones(),
        grays(),
        rc_font_set(false) { }

    struct celpage {
        int w, h, cw, ch;
        SDL_Surface *s;
        bool magentic;
        int start_index;

        celpage(int w, int h, int cw, int ch, SDL_Surface *s, bool m, int si):
            w(w), h(h), cw(cw), ch(ch), s(s), magentic(m), start_index(si) {}
        ~celpage() { }

        bool operator < (celpage r) const { return h < r.h; }
    };

    std::forward_list<celpage> pages;
    std::forward_list<std::pair<long, long>> clones;  // (index, cloned_from)
    std::forward_list<long> grays;
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
    std::fstream dump;
    fn += ".png";
    SDL_SavePNG(ta->album, fn.c_str());
    fn = name;
    fn += ".index";
    dump.open(fn.c_str(), std::ios::out);
    dump<<"count: "<<ta->count<<"\n";
    dump<<"height: "<<ta->height<<"\n";
    for (unsigned i=0; i < ta->count ; i++ ) {
        df_taindex_entry_t& e = ta->index[i];
        dump<<i<<' '<<e.rect.w<<'x'<<e.rect.h<<'+'<<e.rect.x<<'+'<<e.rect.y;
        if (e.magentic)
            dump<<" m";
        if (e.gray)
            dump<<" g";
        dump<<'\n';
    }
    dump.close();
}

/* C++ ugliness forces some typedef names .. */
typedef std::forward_list<implementation::celpage>::iterator piter_t;
typedef std::forward_list<std::pair<long, long>>::iterator cliter_t;
typedef std::forward_list<long>::iterator griter_t;

/* see fgtestbed fgt.raw.Pageman

   This gets called from renderer thread while the load_* stuff
   gets called from simu thread. There is no sync atm, just hope it
   doesn't crash and burn for now.
*/
df_texalbum_t *implementation::get_album() {
    int album_w = 1024; // it's pot; and dumps are manageable in a viewer.
    int w_lcm = 1;

    for (piter_t p(pages.begin()); p != pages.end(); p++)
        w_lcm = lcm(w_lcm, p->cw);

    pages.sort(); // in order of ascending cel-height.

    df_texalbum_t *rv = (df_texalbum_t *) calloc(1, sizeof(df_texalbum_t));
    rv->count = next_index;
    rv->index = (df_taindex_entry_t *) calloc(rv->count, sizeof(df_taindex_entry_t));

    rv->album = beloved_surface(album_w, album_w / 8);

    album_w -= album_w % w_lcm; // actual surface width stays power-of-two.


    int cx = 0, cy = 0, row_h = pages.begin()->ch;

    for (piter_t p(pages.begin()); p != pages.end(); p++) {
        unsigned index = p->start_index;
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
        if (p->ch > row_h) {
            if ( wasted < (p->ch - row_h) * cx ) {
                cx = 0;
                cy += row_h;
            }
            row_h = p->ch;
        } else {
            if ( wasted < (album_w - cx) * (row_h - p->ch) ) {
                cx = 0;
                cy += row_h;
                row_h = p->ch;
            }
        }

        /*  This can produce a false positive if p->ch < row_h
            and we decided to not start new row. */
        rv->album = grow_album(rv->album, cy + row_h);

        for (int j = 0; j < p->h; j++) {
            for (int i = 0; i < p->w; i++) {
                /* Another possible optimization would be to blit entire series
                   of horizontally-adjacent cels when they fit into the current
                   album row. Don't think it is worth the code complexity though. */
                if (cx >= album_w) { /* begin new row. */
                    cx = 0;
                    cy += row_h;   /* grow by old row_h which might be > than current page's ch */
                    row_h = p->ch; /* thus set row_h in case we did not at the start of the page */
                    rv->album = grow_album(rv->album, cy + row_h);
                }
                SDL_Rect src = { i*p->cw, j*p->ch, p->cw, p->ch };
                rv->index[index].rect.x = cx;
                rv->index[index].rect.y = cy;
                rv->index[index].rect.w = p->cw;
                rv->index[index].rect.h = p->ch;
                SDL_BlitSurface(p->s, &src, rv->album, &rv->index[index].rect);
                rv->index[index].magentic = p->magentic;
                cx += p->cw;
                index ++;
            }
        }
    }

    /* do this instead of shrink */
    rv->height = cy + row_h;

    /* execute clone requests */
    for (cliter_t ci(clones.begin()); ci != clones.end(); ci++)
        rv->index[ci->first] = rv->index[ci->second];

    /* execute grayscaling requests */
    for (griter_t gi(grays.begin()); gi != grays.end(); gi++)
        rv->index[*gi].gray = true;

    dump_album(rv, "texalbum");
    return rv;
}

void implementation::release_album(df_texalbum_t *a) {
    SDL_FreeSurface(a->album);
    free(a->index);
    free(a);
}

long implementation::clone_texture(long src) {
    clones.push_front(std::pair<long, long>(next_index, src));
    return next_index++;
}

void implementation::grayscale_texture(long pos) {
    grays.push_front(pos);
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
    pages.emplace_front(dimx, dimy, *disp_x, *disp_y, s, convert_magenta, next_index);

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

    pages.emplace_front(1, 1, s->w, s->h, s, convert_magenta, next_index);
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

    pages.emplace_front(16, 16, s->w/16, s->h/16, s, true, 0);
    rc_font_set = true;
    next_index = 256;
    logr->trace("set_rcfont(): %dx%d", s->w/16, s->h/16);
}

void implementation::reset() {
    rc_font_set = false;
    next_index = 0;
    pages.clear(); /* do SDL_FreeSurface() here since gcc does meddle with object lifetime in sort() */
    clones.clear();
    grays.clear();
    logr->trace("reset()");
}

void implementation::delete_texture(long i) { logr->warn("delete_texture(%ld)", i);}

void implementation::release() { }

static implementation *impl = NULL;

extern "C" DFM_EXPORT itextures * DFM_APIEP gettextures(void) {
    if (!impl) {
        platform = _getplatform();
        logr = platform->getlogr("sdl.textures");
        impl = new implementation();
    }
    return impl;
}
} /* ns */
