#if !defined(ISIMULOOP_H)
#define ISIMULOOP_H

#include "itypes.h"

/* interface used to control the simulation loop */
typedef char (*mainloop_foo_t)();
typedef void (*render_things_foo_t)();
typedef void (*assimilate_buffer_foo_t)(df_buffer_t *);
typedef void (*add_input_event_foo_t)(df_input_event_t *);

struct isimuloop : public imessagesender {
    virtual void release() = 0;
    
    virtual void set_callbacks(mainloop_foo_t, 
                       render_things_foo_t, 
                       assimilate_buffer_foo_t, 
                       add_input_event_foo_t) = 0;

    virtual void start() = 0;
    virtual void join() = 0;
    virtual void render() = 0;

    /* FPS values are passed around as their inverse, in milliseconds. */
    virtual void set_target_sfps(uint32_t) = 0; // mainloop() calls 
    virtual void set_target_rfps(uint32_t) = 0; // render_things() calls, buffer submission.
    
    virtual uint32_t get_actual_sfps() = 0; 
    virtual uint32_t get_actual_rfps() = 0;
    
    /* input event sinks */
    virtual void add_input_event(df_input_event_t *) = 0;

    /* frames done counter */
    virtual uint32_t get_frame_count() = 0;
};

#if defined (DFMODULE_BUILD) || defined(DFMODULE_IMPLICIT_LINK)
extern "C" DECLSPEC isimuloop * APIENTRY getsimuloop(void);
#else // using glue and runtime loading.
extern "C" DECLSPEC isimuloop * APIENTRY (*getsimuloop)(void);
#endif

#endif