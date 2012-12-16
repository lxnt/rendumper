#include <cstring>
#include "iplatform.h"

#define DFMODULE_BUILD
#include "itextures.h"

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

    long index;
    iplatform *platform;
    implementation() : index(0) { platform = getplatform(); }
};

long implementation::clone_texture(long src) {
    index++;
    platform->log_info("stub_textures::clone_texture(%ld) -> %ld", src, index);
    return index;
    /* Just so I don't forget: when cloning, record the fact, rather than
       copy data; it's likely to be immediately grayscaled. */
}

void implementation::grayscale_texture(long pos) {
    platform->log_info("stub_textures::grayscale_texture(%ld)", pos);
}

void implementation::load_multi_pdim(const char *filename, long *tex_pos, long dimx, long dimy,
			       bool convert_magenta, long *disp_x, long *disp_y) {

    platform->log_info("stub_textures::load_multi_pdim(%s, %p, %ld, %ld, %d %p, %p)",
			filename, tex_pos, dimx, dimy, convert_magenta, disp_x, disp_y);
    memset(tex_pos, 0, dimx*dimy*sizeof(long));
    index += dimx*dimy;
}

long implementation::load(const char *filename, bool convert_magenta) {
    index++;
    platform->log_info("stub_textures::load(%s, %d) -> %ld", filename, convert_magenta, index);
    return index;
}

void implementation::delete_texture(long pos) {
    platform->log_info("stub_textures::load(%ld)", pos);
}

df_texalbum_t *implementation::get_album() {
    platform->log_info("stub_textures::get_album()");
    return NULL;
}

void implementation::release_album(df_texalbum_t *p) {
    platform->log_info("stub_textures::release_album(%p)", p);
}


void implementation::release() { }

static implementation *impl = NULL;
extern "C" DECLSPEC itextures * APIENTRY gettextures(void) {
    if (!impl)
        impl = new implementation();
    return impl;
}
} /* ns */
