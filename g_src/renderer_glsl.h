#include "enabler.h"
#ifndef R_GLSL_H
#define R_GLSL_H

struct glsl_private;

class renderer_glsl : public renderer {
    glsl_private *self;
public:
    virtual void accept_textures(textures& );
    virtual void display() {};
    virtual void update_tile(int x, int y) {};
    virtual void update_all() {};
    virtual void render();
    virtual void set_fullscreen() { zoom(zoom_fullscreen); };
    virtual void swap_arrays() {};
    virtual void zoom(zoom_commands cmd);
    virtual void resize(int w, int h);
    virtual void grid_resize(int new_grid_w, int new_grid_h);

    renderer_glsl();
    virtual bool get_mouse_coords(int &x, int &y);
    virtual bool uses_opengl() { return true; }
    virtual ~renderer_glsl();
};

#endif
