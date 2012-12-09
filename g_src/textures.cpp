#include <cassert>

#include "enabler.h"
#include "itextures.h"

#define DFM_STUB(foo) getplatform()->log_error("Stub '%s' called.\n", #foo)

void textures::upload_textures() {
    DFM_STUB(textures::upload_textures);
}
void textures::remove_uploaded_textures() {
    DFM_STUB(textures::remove_uploaded_textures);
}
void *textures::get_texture_data(long) {
    DFM_STUB(textures::get_texture_data);
    return NULL;
}
long textures::clone_texture(long src) {
    return gettextures()->clone_texture(src);
}
void textures::grayscale_texture(long pos) {
    gettextures()->grayscale_texture(pos);
}
void textures::load_multi_pdim(const string &filename, long *tex_pos, long dimx,
			       long dimy, bool convert_magenta,
			       long *disp_x, long *disp_y) {
    gettextures()->load_multi_pdim(filename.c_str(), tex_pos, dimx, dimy, convert_magenta, disp_x, disp_y);
}
long textures::load(const string &filename, bool convert_magenta) {
    return gettextures()->load(filename.c_str(), convert_magenta);
}
void textures::delete_texture(long pos) {
    return gettextures()->delete_texture(pos);
}
long textures::add_texture(void*) {
    DFM_STUB(textures::add_texture);
    return -23;
}
int textures::textureCount() {
    DFM_STUB(textures::textureCount);
    return -23;
}
