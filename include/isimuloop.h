#if !defined(ISIMULOOP_H)
#define ISIMULOOP_H

#include "itypes.h"

/* The align parameter of isimuloop::add_string(). */
#define DF_TEXTALIGN_LEFT     0
#define DF_TEXTALIGN_RIGHT    1
#define DF_TEXTALIGN_CENTER   2
#define DF_MONOSPACE_LEFT     3       // bypass ttf
#define DF_TEXTALIGN_JUSTIFY -100500  // doesn't exist.

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

    /* entry point for the ttf support - see itypes.h for the align values */
    virtual int add_string(const char *str, const char *attrs, int len, int x, int y, int align, int space) = 0;

    /* input event sink */
    virtual void add_input_event(df_input_event_t *) = 0;

    /* frames done counter */
    virtual uint32_t get_frame_count() = 0;
};

#if defined (DFMODULE_BUILD)
extern "C" DFM_EXPORT isimuloop * DFM_APIEP getsimuloop(void);
#else // using glue and runtime loading.
extern getsimuloop_t getsimuloop;
#endif

#endif