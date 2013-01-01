
#include "iplatform.h"

#include "init.h"
#include "enabler.h"
#include "renderer.h"
#include "irenderer.h"

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

bool renderer_offscreen::init_video(int, int)       { DFM_STUB(renderer_offscreen::~init_video); return true; }
renderer_offscreen::~renderer_offscreen()           { DFM_STUB(renderer_offscreen::~renderer_offscreen); }

renderer_offscreen::renderer_offscreen(int grid_w, int grid_h)    {
    stubs_logr->info("renderer_offscreen(%d, %d); gps.dimxy %dx%d", grid_w, grid_h, gps.dimx, gps.dimy);
    world = getrenderer()->get_offscreen_buffer(grid_w, grid_h);
}
void renderer_offscreen::update_all(int offset_x, int offset_y) {
    /* okay, this works the following way:
        DF renderes a part the world into an existing gps object, with its
        dimensions and backing df_buffer_t, then calls this function
        to effectively blit gps' contents into the offscreen buffer
        at the given offset. Clipping is being done by choosing appropriate
        offset by the caller, but we have to be safe here anyway.
    */

    if ((offset_x >= world->w) || (offset_y >= world->h)) {
        stubs_logr->error("update_all(%dx%d): world %ux%u, skipping.", offset_x, offset_x, world->w, world->h);
        return;
    }

    int blit_w = offset_x + gps.dimx > world->w ? world->w - offset_x : gps.dimx;
    int blit_h = offset_y + gps.dimy > world->h ? world->h - offset_y : gps.dimy;

    stubs_logr->info("update_all(%d, %d); gps.dimxy %dx%d, blit_wh %dx%d",
                        offset_x, offset_y, gps.dimx, gps.dimy, blit_w, blit_h);
    /* aw, again this column-wisery */
    for(int x = offset_x; x < offset_x + blit_w ; x++) {
        unsigned offs = x * world->h;
        memcpy(world->screen + offs*4,  gps.screen,                 blit_h * 4);
        memcpy(world->texpos + offs,    gps.screentexpos,           blit_h * 4);
        memcpy(world->addcolor + offs,  gps.screentexpos_addcolor,  blit_h);
        memcpy(world->grayscale + offs, gps.screentexpos_grayscale, blit_h);
        memcpy(world->cf + offs,        gps.screentexpos_cf,        blit_h);
        memcpy(world->cbr + offs,       gps.screentexpos_cbr,       blit_h);
    }
}

void renderer_offscreen::save_to_file(const std::string &s) {
    stubs_logr->info("save_to_file(%s)", s.c_str());
    getrenderer()->export_offscreen_buffer(world, s.c_str());
    world = NULL;
}
