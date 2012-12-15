#include <cstring>
#include "iplatform.h"

#include "SDL.h"

#define DFMODULE_BUILD
#include "itextures.h"

/*
    This is a GL-oriented SDL texture album, optimized for loading speed.
    Thus, convert_magenta, clone and grayscale operations are not done,
    only recorded in the index, assumed to be offloaded to the pixel shader.
    The album is also not shrunk upon filling.
*/

namespace {

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

    /* --- */

    long next_index; // == .size(); next unused index.
    iplatform *platform;
    implementation() : next_index(0), raws(), clones(), grayscaled(), cmax(0), cmay(0) { platform = getplatform(); }

    struct celpage {
        int w, h, cw, ch;
        SDL_Surface *s;
        bool magentic;
        int start_index;

        celpage(int w, int h, int cw, int ch, SDL_Surface s, bool m, int si):
            w(w), h(h), cw(cw), ch(ch), s(s), magentic(m), start_index(fi) {}

        bool operator<(celpage& l, celbage& r) { return l.h < r.h; }
    };

    std::forward_list<celpage> pages;
    std::forward_list<std::pair<long, long>> clones;  // (index, cloned_from)
    std::forward_list<long> grays;
    int cmax, cmay;
};

/* pieces of fgt.gl.rgba_surface */
SDL_Surface *beloved_surface(int w, int h) {
    Uint32 Rmask, Bmask, Amask, Gmask, bpp;

    SDL_PixelFormatEnumToMasks(SDL_PIXELFORMAT_ABGR8888,
        &bpp, &Rmask, &Gmask, &Bmask, &Amask);

    SDL_Surface *s = SDL_CreateRGBSurface(0, w, h,
        bpp, Rmask, Gmask, Bmask, Amask);

    SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_NONE);

    return s;
}

/* make sure it's of our belowed pixelformat and blendmode is none */
SDL_Surface *load_normalize(const char *filename) {
    SDL_Surface *s = IMG_Load(filename);
    if (!s)
        platform->log_fatal("IMG_Load(%s) failed : %s", filename, SDL_GetError());

    SDL_Surface *d = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0);

    if (!d)
        platform->log_fatal("SDL_ConvertSurfaceFormat() failed : %s", SDL_GetError());

    SDL_SetSurfaceBlendMode(d, SDL_BLENDMODE_NONE);
    return d;
}


int gcd(int n, int m) { return m == 0 ? n: gcd(m, n%m); }
int lcm(int a, int b) { return a * b / gcd(a, b); }

SDL_Surface *grow_album(SDL_Surface *album, int min_h, bool fill = false) {
    if (min_h > album_h) { // grow it
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

/*  hmmm. GL_EXT_texture RGBA16_EXT - 16-bit components ? test its resolution. */

/* see fgtestbed fgt.raw.Pageman */
df_texalbum_t *implementation::get() {
    int album_w = 1024; // it's pot; and dumps are manageable in a viewer.
    int w_lcm = 1;

    for (pages.iterator p = pages.begin(); p != pages.end(); p++) {
        w_lcm = lcm(w_lcm, p->cw);
        count += p->w * p->h;
    }

    pages.sort(); // in order of ascending cel-height.

    album_w -= album_w % w_lcm;

    df_texalbum_t *rv = (df_texalbum_t *) calloc(1, sizeof(df_texalbum_t));
    rv->count = next_index;
    rv->index = (df_taindex_entry_t *) calloc(rv->count, sizeof(df_taindex_entry_t));

    rv->album = beloved_surface(album_w, album_w / 8);
    int pref_page_cel_count = 0;

    int cx = 0, cy = 0, row_h = pages[0].ch;

    for (pages.iterator p = pages.begin(); p != pages.end(); p++) {
        index = p.start_index;
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

        int wasted = (album_w - cx) * old_row_h; // when starting new row
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
                rv->index[index].rect = { cx, cy, p->cw, p->ch };
                SDL_BlitSurface(p->s, &src, album, &dst);
                rv->index[index].magentic = magentic;
                cx += p->cw;
                index ++;
            }
        }
    }

    /* do this instead of shrink */
    rv->height = cy + row_h;

    /* execute clone requests */
    for (clones.iterator ci = clones.begin(); ci != clones.end(); ci++)
        rv->index[ci->first] = rv->index[ci->second];

    /* execute grayscaling requests */
    for (grays.iterator gi = grays.begin(); gi != grays.end(); gi++)
        rv->index[*gi].gray = true;

    return rv;
}

void implementation::release_album(df_texalbum_t *a) {
    SDL_FreeSurface(a->album);
    free(a->index);
    free(a);
}

long implementation::clone_texture(long src) {
    clones.insert(std::pair<long, long>(index, src));
    return next_index++;
}

void implementation::grayscale_texture(long pos) {
    grayscaled.insert(pos);
}

void implementation::load_multi_pdim(const char *filename, long *tex_pos, long dimx, long dimy,
			       bool convert_magenta, long *disp_x, long *disp_y) {

    SDL_Surface *s = load_normalize(filename);
    *disp_x = s.w/dimx, *disp_y = s.h/dimy
    pages.emplace_front(dimx, dimy, *disp_x, *disp_y, s, convert_magenta, next_index);

    for (int i = 0; i < dimx*dimy; i++)
        tex_pos[next_index - start] = next_index++;

    if (*disp_x > cmax) cmax = *dispx;
    if (*disp_y > cmay) cmay = *dispy;
}

long implementation::load(const char *filename, bool convert_magenta) {
    SDL_Surface *s = load_normalize(filename);
    if (!crap) // oh, shit, go die.
        platform->log_fatal("failed to load %s", filename);

    pages.emplace_front(1, 1, s.w, s.h, s, convert_magenta, next_index);

    return next_index++;
}

void implementation::delete_texture(long pos) { /* fuck off and die. */ }

void implementation::release() { } // also. forget allocations, too.

static implementation *impl = NULL;
extern "C" DECLSPEC itextures * APIENTRY gettextures(void) {
    if (!impl) {
        IMG_init(0xF); // well, at least we'll get BMP and company.
        impl = new implementation();
    }
    return impl;
}
} /* ns */
