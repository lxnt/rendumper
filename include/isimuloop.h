#if !defined(ISIMULOOP_H)
#define ISIMULOOP_H

#include "itypes.h"

/* interface used to control the simulation loop */
typedef char (*mainloop_foo_t)();
typedef void (*render_things_foo_t)();
typedef void (*assimilate_buffer_foo_t)(df_buffer_t *);
typedef void (*add_input_event_foo_t)(df_input_event_t *);

struct isimuloop : public imessagesender {
    enum justi_t : uint8_t {
        justify_left,
        justify_center,
        justify_right,
        justify_cont,
        not_truetype
    };

    virtual void release() = 0;

    virtual void set_callbacks(mainloop_foo_t,
                       render_things_foo_t,
                       assimilate_buffer_foo_t,
                       add_input_event_foo_t) = 0;

    /* Entirely unbeautiful interface between the gps and the currently assimilated buffer */

    /* TTF support. No feedback. Strings never wrap. */
    /* attrs: see below */
    virtual void add_string(const char *s, unsigned x, unsigned y, unsigned space, uint8_t attr, justi_t justi) = 0;

    /* attrs are byte per character is *s. fg = attr&7; bg = (attr>>3)&7, br = attr>>6 */
    virtual void add_attred_string(const char *s, unsigned x, unsigned y, const uint8_t *attrs) = 0;

    /* FX support */
    virtual void make_it_rain(unsigned x, unsigned y) = 0;
    virtual void make_it_snow(unsigned x, unsigned y) = 0;
    virtual void make_it_dim(unsigned x, unsigned y, unsigned dim) = 0;

    /* thread control */
    virtual void start() = 0;
    virtual void join() = 0;

    /* kickpoint */
    virtual void render() = 0;

    /* FPS values are passed around as their inverse, in milliseconds -FIX THIS SOMEHOW. */
    virtual void set_target_sfps(uint32_t) = 0; // mainloop() calls
    virtual void set_target_rfps(uint32_t) = 0; // render_things() calls, buffer submission.

    virtual uint32_t get_actual_sfps() = 0;
    virtual uint32_t get_actual_rfps() = 0;

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