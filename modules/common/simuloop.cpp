#include <cstdarg>

#include "iplatform.h"
#include "imqueue.h"
#include "irenderer.h"
#include "emafilter.h"

#define DFMODULE_BUILD
#include "isimuloop.h"

namespace {

struct implementation : public isimuloop {
    void release();

    void set_callbacks(mainloop_foo_t,
                       render_things_foo_t,
                       assimilate_buffer_foo_t,
                       add_input_event_foo_t);

    void start();
    void join();
    void render();

    void set_target_sfps(uint32_t);
    void set_target_rfps(uint32_t);

    uint32_t get_actual_sfps();
    uint32_t get_actual_rfps();

    void add_input_event(df_input_event_t *);

    uint32_t get_frame_count();

    void acknowledge(const itc_message_t&) { /* we don't have to acknowledge anything */ }

    /* --- */
    void simulation_thread();
    thread_t thread_id;

    mainloop_foo_t mainloop_cb;
    render_things_foo_t render_things_cb;
    assimilate_buffer_foo_t assimilate_buffer_cb;
    add_input_event_foo_t add_input_event_cb;

    bool started;
    bool paused;
    bool pedal_to_the_metal;
    uint32_t target_mainloop_ms;
    uint32_t target_renderth_ms;

    /* instrumentation */

    uint32_t frames;   // no. of mainloop() calls
    uint32_t renders;  // no. of render_things() calls

    ema_filter_t mainloop_time_ms;
    ema_filter_t render_things_time_ms;

    ema_filter_t mainloop_period_ms;
    ema_filter_t render_things_period_ms;

    /* ITC */
    imqd_t incoming_q; // stuff from renderer or whoever

    /* dependencies */
    iplatform *platform;
    imqueue *mqueue;
    irenderer *renderer;

    implementation() :
                      started(false),
                      paused(false),
                      pedal_to_the_metal(false),
                      target_mainloop_ms(20),
                      target_renderth_ms(20),
                      frames(0),
                      renders(0),
                      mainloop_time_ms(0.025, 10.0, 5),
                      render_things_time_ms(0.025, 10.0, 5),
                      mainloop_period_ms(0.025, 10.0, 5),
                      render_things_period_ms(0.025, 10.0, 5)
                       {
        platform = getplatform();
        mqueue = getmqueue();
    }
};

void implementation::set_callbacks(mainloop_foo_t ml,
                   render_things_foo_t rt,
                   assimilate_buffer_foo_t ab,
                   add_input_event_foo_t ain) {
    mainloop_cb = ml;
    render_things_cb = rt;
    assimilate_buffer_cb = ab;
    add_input_event_cb = ain;
}

uint32_t implementation::get_frame_count() { return frames; }

/* to be called from other threads. */
void implementation::add_input_event(df_input_event_t *event) {
    itc_message_t msg;
    msg.t = itc_message_t::input_event;
    msg.d.event = *event;
    mqueue->copy(incoming_q, &msg, sizeof(msg), -1);
}

/* to be called from other threads. */
void implementation::render() {
    itc_message_t msg;
    msg.t = itc_message_t::render;
    mqueue->copy(incoming_q, &msg, sizeof(msg), -1);
}

static int thread_stub(void *owner) {
    ((implementation *)owner)->simulation_thread();
    return 0;
}

void implementation::start() {
    if (started) {
        platform->log_error("second simuloop start ignored");
        return;
    }
    started = true;
    platform->thread_create(thread_stub, "simulation", (void *)this);
}

void implementation::join() {
    if (!started) {
        platform->log_error("join(): not started");
        return;
    }
    platform->thread_join(thread_id, NULL);
}

void implementation::set_target_sfps(uint32_t fps) {
    if (!fps) {
        pedal_to_the_metal = true;
    } else {
        pedal_to_the_metal = false;
        target_mainloop_ms = 1000/fps;
    }
}

void implementation::set_target_rfps(uint32_t fps) {
    if (fps)
        target_renderth_ms = 1000/fps;
}

uint32_t implementation::get_actual_sfps() { return mainloop_period_ms.get(); };
uint32_t implementation::get_actual_rfps() { return render_things_period_ms.get(); }


/* for the time being ignore render_things and mqueue overhead when
   limiting fps.

    does mainloop() touch gps/buffer ?

    also need to get rid of render message. it is not needed.
*/
void implementation::simulation_thread() {
    renderer = getrenderer();

    incoming_q  = mqueue->open("simuloop", 1<<10);

    /* assimilate initial buffer so gps' ptrs stay valid all the time */
    df_buffer_t *renderbuf = renderer->get_buffer();
    platform->log_info("got renderbuf %dx%d", renderbuf->w, renderbuf->h);
    assimilate_buffer_cb(renderbuf);


    uint32_t last_renderth_at = 0;
    uint32_t last_mainloop_at = 0;
    int32_t read_timeout_ms = 0;
    while (true) {
        bool force_renderth = false;

        //platform->log_info("read_timeout_ms=%d", read_timeout_ms);
        while (true) {
            void *buf; size_t len;
            int rv = mqueue->recv(incoming_q, &buf, &len, read_timeout_ms);
            read_timeout_ms = 0; // reset timeout so we don't wait second time.

            if (rv == IMQ_TIMEDOUT)
                break;

            if (rv != 0)
                platform->fatal("simuloop(): %d from mqueue->recv().");

            itc_message_t *msg = (itc_message_t *)buf;

            switch (msg->t) {
                case itc_message_t::pause:
                    paused = true;
                    msg->sender->acknowledge(*msg);
                    mqueue->free(msg);
                    break;

                case itc_message_t::start:
                    paused = false;
                    msg->sender->acknowledge(*msg);
                    mqueue->free(msg);
                    break;

                case itc_message_t::render:
                    force_renderth = true;
                    mqueue->free(msg);
                    break;

                case itc_message_t::set_fps:
                    target_mainloop_ms = 1000.0 / msg->d.fps;
                    mqueue->free(msg);
                    break;

                case itc_message_t::input_event:
                    add_input_event_cb(&(msg->d.event));
                    mqueue->free(msg);
                    break;

		default:
		    platform->log_error("simuloop(): unexpected message type %d", msg->t);
                    mqueue->free(msg);
		    break;
            }
        }

        if (paused) { // why do we need this pause functionality at all ?
                      // Maybe to sync with dfhack running in a separate thread?
                      // Or a FG renderer sifting through the guts?
            read_timeout_ms = -1;
            continue;
        }

        uint32_t events_done_at = platform->GetTickCount();
        //platform->log_info("%d or %d + %d <= %d", pedal_to_the_metal, last_mainloop_at, target_mainloop_ms, events_done_at);
        if (pedal_to_the_metal or (last_mainloop_at + target_mainloop_ms <= events_done_at)) {
            uint32_t ml_start_ms = events_done_at;
            if (mainloop_cb()) {
                renderer->simuloop_quit();
                platform->log_info("simuloop quit.");
                return;
            }
            frames ++;
            uint32_t ml_end_ms = platform->GetTickCount();
            mainloop_period_ms.update(ml_end_ms - last_mainloop_at);
            mainloop_time_ms.update(ml_end_ms - ml_start_ms);
            last_mainloop_at = ml_end_ms;
            //platform->log_info("frame %d; %d ms", frames, ml_end_ms - ml_start_ms);
        }

        if ((platform->GetTickCount() - last_renderth_at) > target_renderth_ms)
            force_renderth = true;

        if (force_renderth) {
            force_renderth = false;
            uint32_t rt_start_ms = platform->GetTickCount();
            render_things_cb();
            renderer->submit_buffer(renderbuf);
            renderbuf = renderer->get_buffer();
            assimilate_buffer_cb(renderbuf);
            renders ++;
            uint32_t rt_end_ms = platform->GetTickCount();
            render_things_period_ms.update(rt_end_ms - last_renderth_at);
            render_things_time_ms.update(rt_end_ms - rt_start_ms);
            last_renderth_at = rt_end_ms;
        }

        /* here calculate how long can we sleep in mq->read()
            1. next renderth is expected at last_renderth_at + target_renderth_ms;
                that is, in (last_renderth_at + target_renderth_ms - now), might be negative.
            2. next mainloop is expected at last_mainloop_at + target_mainloop_ms
                that is, in (last_mainloop_at + target_mainloop_ms - now), might be negative.
                or RIGHT NOW if FPS==MAX. */

        if (pedal_to_the_metal) {
            read_timeout_ms = 0;
            continue;
        }

        uint32_t now = platform->GetTickCount();
        int next_renderth_in = last_renderth_at + target_renderth_ms - now;
        int next_mainloop_in = last_mainloop_at + target_mainloop_ms - now;
        //platform->log_info("next_renderth_in %d next_mainloop_in %d", next_renderth_in, next_mainloop_in);
        if ((next_renderth_in < 0) or (next_mainloop_in < 0)) {
            read_timeout_ms = 0;
            continue;
        }

        /* the following obvously can result in negative number which must be clamped to zero */
        read_timeout_ms = next_renderth_in > next_mainloop_in ? next_mainloop_in : next_renderth_in;
        read_timeout_ms = read_timeout_ms < 0 ? 0 : read_timeout_ms;
    }
}


void implementation::release() { }

static implementation *impl = NULL;
extern "C" DECLSPEC isimuloop * APIENTRY getsimuloop(void) {
    if (!impl)
        impl = new implementation();
    return impl;
}
} /* ns */
