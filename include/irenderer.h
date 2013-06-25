#if !defined (IRENDERER_H)
#define IRENDERER_H

#include "itypes.h"

struct irenderer : public imessagesender {
    virtual void release(void) = 0;

    /* this is the interface used from the simulation thread
       via enablerst/enabler_inputst methods. */

    virtual void zoom_in() = 0;
    virtual void zoom_out() = 0;
    virtual void zoom_reset() = 0;
    virtual void toggle_fullscreen() = 0;
    virtual void override_grid_size(unsigned, unsigned) = 0;
    virtual void release_grid_size() = 0;
    virtual int mouse_state(int *mx, int *my) = 0; // ala sdl2's one.

    virtual uint32_t get_gridsize() = 0;
    virtual void set_gridsize(unsigned, unsigned) = 0;

    virtual df_buffer_t *get_buffer() = 0;
    virtual void submit_buffer(df_buffer_t *buf) = 0;
    virtual df_buffer_t *get_offscreen_buffer(unsigned, unsigned) = 0;
    virtual void export_offscreen_buffer(df_buffer_t *, const char *) = 0;

    /* basically, if current Pszy == ttf_lineheight */
    virtual int ttf_active() = 0;
    /* how many grid cells will the given string touch, and return data via pointers */
    virtual int ttf_gridwidth(const uint16_t *, const uint32_t,
                            uint32_t *, uint32_t *, uint32_t *, uint32_t *, uint32_t *) = 0;

    /* starts the renderer thread. */
    virtual void start() = 0;
    virtual void join() = 0;

    /* runs the loop in the current thread */
    virtual void run_here() = 0;

    /* notifies of */
    virtual void simuloop_quit() = 0;

    /* signals to grab new texalbum */
    virtual void reset_textures() = 0;
};

#if defined (DFMODULE_BUILD)
extern "C" DFM_EXPORT irenderer * DFM_APIEP getrenderer(void);
#else // using glue and runtime loading.
extern getrenderer_t getrenderer;
#endif

#endif
