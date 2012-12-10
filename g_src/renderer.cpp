
#include "iplatform.h"
#define DFM_STUB(foo) getplatform()->log_error("Stub '%s' called.\n", #foo)

#include "init.h"
#include "enabler.h"
#include "renderer.h"

Either<texture_fullid,texture_ttfid> renderer::screen_to_texid(int , int ) {
    DFM_STUB(renderer::screen_to_texid);
    texture_ttfid ret = 0;
    return Either<texture_fullid,texture_ttfid>(ret);
}

void renderer::display()                { DFM_STUB(renderer::display); }
bool renderer::uses_opengl()            { DFM_STUB(renderer::uses_opengl); return false; }
void renderer::cleanup_arrays()         { DFM_STUB(renderer::cleanup_arrays); }
void renderer::gps_allocate(int, int)   { DFM_STUB(renderer::gps_allocate); }
void renderer::swap_arrays()            { DFM_STUB(renderer::swap_arrays); }
void renderer::set_fullscreen()         { DFM_STUB(renderer::set_fullscreen); }
void renderer::zoom(zoom_commands)      { DFM_STUB(renderer::zoom); }
renderer::renderer()                    { DFM_STUB(renderer::renderer); }
renderer::~renderer()                   { DFM_STUB(renderer::~renderer); }

void *renderer_2d_base::tile_cache_lookup(texture_fullid &, bool) {
        DFM_STUB(renderer_2d_base::tile_cache_lookup); return NULL; }
bool renderer_2d_base::init_video(int, int)     { DFM_STUB(renderer_2d_base::init_video); return true; }
void renderer_2d_base::update_tile(int, int)    { DFM_STUB(renderer_2d_base::update_tile); }
void renderer_2d_base::update_all()             { DFM_STUB(renderer_2d_base::update_all); }
void renderer_2d_base::render()                 { DFM_STUB(renderer_2d_base::render); }
renderer_2d_base::~renderer_2d_base()           { DFM_STUB(renderer_2d_base::~renderer_2d_base); }
void renderer_2d_base::grid_resize(int, int)    { DFM_STUB(renderer_2d_base::grid_resize); }
renderer_2d_base::renderer_2d_base()            { DFM_STUB(renderer_2d_base::renderer_2d_base); }
void renderer_2d_base::compute_forced_zoom() { DFM_STUB(renderer_2d_base::compute_forced_zoom); }
std::pair<int, int> renderer_2d_base::compute_zoom(bool) {
    DFM_STUB(renderer_2d_base::compute_zoom);
    return std::pair<int, int>(0,0); }
void renderer_2d_base::resize(int , int)        { DFM_STUB(renderer_2d_base::resize); }
void renderer_2d_base::reshape(pair<int,int>)   { DFM_STUB(renderer_2d_base::reshape); }
void renderer_2d_base::set_fullscreen()         { DFM_STUB(renderer_2d_base::set_fullscreen); }
bool renderer_2d_base::get_mouse_coords(int &, int &) {
        DFM_STUB(renderer_2d_base::get_mouse_coords); return false; }
void renderer_2d_base::zoom(zoom_commands)      { DFM_STUB(renderer_2d_base::zoom); }


renderer_offscreen::~renderer_offscreen()           { DFM_STUB(renderer_offscreen::~renderer_offscreen); }
renderer_offscreen::renderer_offscreen(int, int)    { DFM_STUB(renderer_offscreen::renderer_offscreen); }
void renderer_offscreen::update_all(int, int)       { DFM_STUB(renderer_offscreen::update_all); }
void renderer_offscreen::save_to_file(const std::string &) { DFM_STUB(renderer_offscreen::save_to_file); }
