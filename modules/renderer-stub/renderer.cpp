struct irenderer {
    virtual void release(void) = 0;

    virtual void zoom_in(void) = 0;
    virtual void zoom_out(void) = 0;
    virtual void zoom_reset(void) = 0;
    virtual void toggle_fullscreen(void) = 0;
    virtual void override_grid_size(void) = 0;
    virtual void release_grid_size(void) = 0; 
    virtual int mouse_state(int *mx, int *my) = 0; // ala sdl2's one.
    
    virtual df_buffer_t *get_buffer(void) = 0;
    virtual void release_buffer(df_buffer_t *buf) = 0;
    
};

#if defined (DFMODULE_BUILD) || defined(DFMODULE_IMPLICIT_LINK)
extern "C" DFM_EXPORT irenderer * DFM_APIEP getrenderer(void);
#else // using glue and runtime loading.
extern "C" DFM_EXPORT irenderer * DFM_APIEP (*getrenderer)(void);
#endif

typedef void (*vvfunc_t)(void);

/* interface used to control the simulation loop */
struct isimuloop {
    virtual void release(void) = 0;
    
    /* supplies the gps, render_things() and mainloop() 
       to the simulation loop object/thread implementation. */
    virtual void set_callbacks(vvfunc_t render_things, vvfunc_t mainloop, vvfunc_t gps_resize);
    
    /* fps control used for movies */
    virtual int  get_fps(void) = 0; // returns actual calculated value (ema).
    virtual void set_fps(int fps) = 0; 

    /* execution control for the direct access to the memory 
       has non-recursive lock semantics */
    virtual void pause(void) = 0; // does not return until the loop is paused
    virtual void unpause(void) = 0;
    
    /* simulation frame counter */
    virtual int simticks(void);
    
    /* if the simulation quitted */
    virtual bool quit(void);
    
    /* instrumentation */
    virtual int render_time(void) = 0;
    virtual int simulate_time(void) = 0; 

};