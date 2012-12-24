#include <cstring>
#include "common_code_deps.h"

#define DFMODULE_BUILD
#include "itextures.h"

namespace {

ilogger *logr = NULL;

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

    long index;
    implementation() : index(0) { }
};

long implementation::clone_texture(long src) {
    index++;
    logr->trace("clone_texture(%ld) -> %ld", src, index);
    return index;
    /* Just so I don't forget: when cloning, record the fact, rather than
       copy data; it's likely to be immediately grayscaled. */
}

void implementation::grayscale_texture(long pos) {
    logr->trace("grayscale_texture(%ld)", pos);
}

void implementation::load_multi_pdim(const char *filename, long *tex_pos, long dimx, long dimy,
			       bool convert_magenta, long *disp_x, long *disp_y) {

    logr->trace("load_multi_pdim(%s, %p, %ld, %ld, %d %p, %p)",
			filename, tex_pos, dimx, dimy, convert_magenta, disp_x, disp_y);
    memset(tex_pos, 0, dimx*dimy*sizeof(long));
    index += dimx*dimy;
}

long implementation::load(const char *filename, bool convert_magenta) {
    index++;
    logr->trace("load(%s, %d) -> %ld", filename, convert_magenta, index);
    return index;
}

void implementation::delete_texture(long pos) {
    logr->trace("delete_texture(%ld)", pos);
}

df_texalbum_t *implementation::get_album() {
    logr->trace("get_album()");
    return NULL;
}

void implementation::release_album(df_texalbum_t *p) {
    logr->trace("release_album(%p)", p);
}

void implementation::set_rcfont(const void *, int) {
    logr->trace("set_rcfont()");
}
void implementation::reset() {
    index = 0;
    logr->trace("reset()");
}


void implementation::release() { }

static implementation *impl = NULL;
extern "C" DFM_EXPORT itextures * DFM_APIEP gettextures(void) {
    if (!impl) {
        _get_deps();
        logr = platform->getlogr("cc.stub.textures");
        impl = new implementation();
    }
    return impl;
}
} /* ns */
