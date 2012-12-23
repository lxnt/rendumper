#if !defined (IRENDERER_H)
#define IRENDERER_H

/* DF renderer modularization. Based on
http://www.codeproject.com/Articles/28969/HowTo-Export-C-structes-from-a-DLL#CppMatureApproach


  Most of the graphicst functions are inlines, used, I guess, by the viewscreenst
   family. making them virtual instead obviously imposes a performance penalty.

    A solution might be conversion of the viewscreenst family to the interfaces instead,
    which will allow to hide (and optimize) their implementation completely.
    problem is there's quite a lot of them, and they, in turn, depend much on more
    df internals. Thus this is unworkable.

    Another solution is to not convert graphicst to an interface at all, leaving
    it inside the blob, and define a screen submission interface instead. However, this
    has its own problem, that is, the TTF support, which can't really work with just the
    screen/texpos arrays.

    This is, in fact, workable. This way libgraphics.so gets split into the renderer and
    the 'rest of it' parts, the renderer being a DLL on windows. Changes to the source code
    are also minimized, and, importantly, this allows development to proceed without
    frequent recompiles of the entire game.

    Okay. So what do we put into the renderer.dll? Obviously, all of the platform-dependent
    code.
     - musicsound : imusicsound.h + wrapper in musicsound.h/musicsound.hpp
     - initst?
     - renderer, obviously
     - the offscreen renderer : ioffscreen TODO.
     - textures. They are in fact married to the renderer, so it's not the textures struct
        as it exists today, but an interface for the game engine to map portions of the
        png files to screen/texpos values and have them loaded at the same time.
     - something for enabler_inputst and keybinding_init to be divorced from SDL constants
        and data types
     - enabler, turned inside out. should accept function pointers to render_things, mainloop,
        etc, so that it can run the main loop.


    offscreen renderer does:
        - create it
        - stuff gps with the map
        - update_all() on the renderer
        - save_to_file() on the renderer.


    Interthread communication.
    --------------------------

    Main threads:

     1. render thread: processes raw input events, control events from
        the simulation thread, renders stuff from the gps object.
     2. simulation thread: calls the mainloop() and the render_things()
        functions. They, in turn, update the gps object, and use the
        enablerst/enabler_inputst code, which is backed by the irenderer
        and ikeyboard interfaces.

    The data flow:

     1. Control events:
      {S: interfacest::loop, etc -> irenderer::zoom_in, etc } -> | -> {R: irenderer implementation }
      This is how zoom_*, etc methods are implemented.

     2. Input events:
        {R event loop -> ikeyboard mapper to DF 'syms'} -> |
            -> { S: ikeyboard::get_input -> simulation (enabler_inputst::add_input, etc }

     3. Data for the renderer - the arrays in the graphicst gps object.
        The render_things() function is understood to fill out the arrays.
        It is the renderer's task to allocate and manage the arrays.
        When the simulation thread needs a buffer to render_things() into,
        it calls irenderer::get_buffer() method. It is guaranteed to return
        an otherwise unused buffer. When it is done with it, and the buffer
        is ready to be displayed, it releases it by calling irenderer::release_buffer()
        on it.

        Who controls grid size? The renderer does. gps.resize() gets called after
        a buffer is acquired, in the simulation thread.

        Who controls the fps and the decision to call the render_things()?

        This is controlled via the isimuloop interface. FPS setting is done by the
        interfacest object via enablerst methods, that is, from the simulation thread itself.

        render_things() call is requested by the renderer when it's time to render
        something, with isimuloop::render() call.

        The dfhack, the fugr_dump and other such stuff runs in separate threads,
        and uses isimuloop::pause()/isimuloop::unpause() calls to control simulation.

    Thread borders:
        irenderer methods are supposed to be called from simulation thread only.


    Simulation thread pseudocode:
        # now() - floating point seconds here, resolution should be
        # bettern that a microsecond.
        ... set_callbacks() called
        done = False
        g_fps_limit = 20
        s_fps_limit = 100
        last_s_frame_at = now() - 1/g_fps_limit
        next_s_frame_at = now()
        last_g_frame_at = now() - 1/s_fps_limit
        while not done:
            g_fps.update( 1 / (now() - last_g_frame_at) )
            last_g_frame_at = now()
            next_g_frame_at = 1/g_fps_limit + last_g_frame_at

            irenderer::get_buffer()
            gps.resize()
            render_things()
            irenderer::release_buffer()

            while True:
                check_for_messages()
                if pause_message:
                    wait_for_unpause_message()
                if set_g_fps_message:
                    g_fps_limit = new_g_fps_limit
                    maybe_break
                if set_s_fps_message:
                    s_fps_limit = new_s_fps_limit
                    maybe_break
                if next_s_frame_at > now():
                    s_fps.update(1/ (now() - last_s_frame_at)
                    last_s_frame_at = now()
                    next_s_frame_at = last_s_frame_at + 1/s_fps_limit
                    if mainloop():
                        done = True
                        break
                    simticks += 1
                if next_g_frame_at > now():
                    break

    Render thread pseudocode:
        while not isimloop.quit():
            pull_input_events()
            pull_messages_from_simloop()
            update_ikeyboard_state()
            update_renderer_state()
            if a_buffer_released:
                render_the_buffer()

*/

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
    virtual void set_gridsize(unsigned _w, unsigned _h) = 0;

    virtual df_buffer_t *get_buffer() = 0;
    virtual void submit_buffer(df_buffer_t *buf) = 0;

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
extern "C" irenderer * DFM_APIEP (*getrenderer)(void);
#endif

#endif
