#include <cstdarg>
#include <cstring>
#include <string>

#include "common_code_deps.h"

#include "emafilter.h"
#include "df_buffer.h"
#include "shrink.h"
#include "df_text.h"

#define DFMODULE_BUILD
#include "isimuloop.h"

namespace {

ilogger *simulogger = NULL;

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

    int add_string(const char *str, const char *attrs, int len, int x, int y, int just, int space);

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

    df_buffer_t *renderbuf;
    ilogger *logr_fps;
    ilogger *logr_bufs;
    ilogger *logr_string;
    ilogger *textlogr;

    irenderer *renderer;

    implementation() :
                      started(false),
                      paused(false),
                      pedal_to_the_metal(false),
                      target_mainloop_ms(20),
                      target_renderth_ms(20),
                      frames(0),
                      renders(0),
                      mainloop_time_ms(0.02, 10.0, 0.5),
                      render_things_time_ms(0.02, 10.0, 0.5),
                      mainloop_period_ms(0.02, 10.0, 0.5),
                      render_things_period_ms(0.02, 10.0, 0.5),
                      incoming_q(-1),
                      renderbuf(NULL),
                      logr_fps(NULL),
                      logr_bufs(NULL),
                      logr_string(NULL),
                      renderer(NULL)
                       { }
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
    memset(&msg, 0, sizeof(msg)); // appease its valgrindiness
    msg.t = itc_message_t::input_event;
    msg.d.event = *event;
    mqueue->copy(incoming_q, &msg, sizeof(msg), -1);
}

/* to be called from other threads. */
void implementation::render() {
    itc_message_t msg;
    memset(&msg, 0, sizeof(msg)); // appease its valgrindiness
    msg.t = itc_message_t::render;
    mqueue->copy(incoming_q, &msg, sizeof(msg), -1);
}

static int thread_stub(void *owner) {
    ((implementation *)owner)->simulation_thread();
    return 0;
}

void implementation::start() {
    if (started) {
        simulogger->error("second simuloop start ignored");
        return;
    }
    started = true;
    thread_id = platform->thread_create(thread_stub, "simulation", (void *)this);
}

void implementation::join() {
    if (!started) {
        simulogger->error("join(): not started");
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
    logr_fps->info("set_target_sfps(%d): target_mainloop_ms=%d", fps, target_mainloop_ms);
}

void implementation::set_target_rfps(uint32_t fps) {
    if (fps)
        target_renderth_ms = 1000/fps;
    else
        target_renderth_ms = 1;
    logr_fps->info("set_target_sfps(%d): target_mainloop_ms=%d", fps, target_renderth_ms);
}

uint32_t implementation::get_actual_sfps() { float pms = mainloop_period_ms.get(); return pms > 1.0 ? 1000/pms : 999; }
uint32_t implementation::get_actual_rfps() { float pms = render_things_period_ms.get(); return pms > 1.0 ? 1000/pms : 999; }


/* for the time being ignore render_things and mqueue overhead when
   limiting fps.

    does mainloop() touch gps/buffer ?

    also need to get rid of render message. it is not needed.
*/
void implementation::simulation_thread() {
    renderer = _getrenderer();
    ilogger *logr = platform->getlogr("cc.simuloop");
    ilogger *logr_timing = platform->getlogr("cc.simuloop.timing");
    logr_bufs = platform->getlogr("cc.simuloop.bufs");
    logr_string = platform->getlogr("cc.simuloop.str");

    incoming_q = mqueue->open("simuloop", 1<<10);

    /* assimilate initial buffer so gps' ptrs stay valid all the time */
    renderbuf = NULL;
    df_buffer_t *backup_buf = allocate_buffer_t(80, 25, 0);
    assimilate_buffer_cb(backup_buf);
    logr->trace("backup_buf is %p", backup_buf);

    uint32_t last_renderth_at = 0;
    uint32_t last_mainloop_at = 0;
    int32_t read_timeout_ms = 0;
    bool force_renderth = false;  // got a command to render (from whom?)
    bool due_renderth = false;    // next frame is due

    while (true) {

        logr_timing->trace("read_timeout_ms=%d", read_timeout_ms);
        while (true) {
            void *buf; size_t len;
            int rv = mqueue->recv(incoming_q, &buf, &len, read_timeout_ms);
            read_timeout_ms = 0; // reset timeout so we don't wait second time.

            if (rv == IMQ_TIMEDOUT)
                break;

            if (rv != 0)
                logr->fatal("%d from mqueue->recv().");

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
		    logr->error("simuloop(): unexpected message type %d", msg->t);
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
        logr_timing->trace("%d or %d + %d <= %d", pedal_to_the_metal, last_mainloop_at, target_mainloop_ms, events_done_at);
        if (pedal_to_the_metal or (last_mainloop_at + target_mainloop_ms <= events_done_at)) {
            uint32_t ml_start_ms = events_done_at;
            if (mainloop_cb()) {
                renderer->simuloop_quit();
                simulogger->info("simuloop quit.");
                return;
            }
            frames ++;
            uint32_t ml_end_ms = platform->GetTickCount();
            mainloop_period_ms.update(ml_end_ms - last_mainloop_at);
            mainloop_time_ms.update(ml_end_ms - ml_start_ms);
            logr_timing->trace("s-frame %d; %d ms work, %d ms period ema_work %f ema_period %f", frames, ml_end_ms - ml_start_ms, ml_end_ms - last_mainloop_at,
            mainloop_time_ms.get(), mainloop_period_ms.get());
            last_mainloop_at = ml_end_ms;
        }

        //logr_timing->trace("last_renderth_at=%d target_renderth_ms=%d", last_renderth_at, target_renderth_ms);
        if ((platform->GetTickCount() - last_renderth_at) >= target_renderth_ms)
            due_renderth = true;

        /* if we're forced to render instead of it being due; do we really
           force a buffer from the renderer, or just skip? */
        if (force_renderth or due_renderth) {
            due_renderth = false;
            if ((renderbuf = renderer->get_buffer()) != NULL) {
                logr_bufs->trace("simuloop(): rendering into buf %p", renderbuf);
                force_renderth = false;
                if (renderbuf->text == NULL)
                    renderbuf->text = new df_text_t;
                assimilate_buffer_cb(renderbuf);
                uint32_t rt_start_ms = platform->GetTickCount();
                render_things_cb();
                uint32_t rt_end_ms = platform->GetTickCount();
                renderer->submit_buffer(renderbuf);
                renderbuf = NULL;
                assimilate_buffer_cb(backup_buf); // so that gps pointers stay valid: is this really needed?

                renders ++;
                render_things_period_ms.update(rt_end_ms - last_renderth_at);
                render_things_time_ms.update(rt_end_ms - rt_start_ms);
                logr_timing->trace("r-frame %d; %d ms work, %d ms period ema_work %f ema_period %f", renders, rt_end_ms - rt_start_ms, rt_end_ms - last_renderth_at,
                     render_things_time_ms.get(), render_things_period_ms.get());
                last_renderth_at = rt_end_ms;
            } else {
                logr_bufs->trace("[%d] graphics frame skipped: no buffer from the renderer", events_done_at);
                last_renderth_at = events_done_at;
            }
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
        //logr_timing->trace("next_renderth_in %d next_mainloop_in %d", next_renderth_in, next_mainloop_in);
        if ((next_renderth_in < 0) or (next_mainloop_in < 0)) {
            read_timeout_ms = 0;
            continue;
        }

        /* the following obvously can result in negative number which must be clamped to zero */
        read_timeout_ms = next_renderth_in > next_mainloop_in ? next_mainloop_in : next_renderth_in;
        read_timeout_ms = read_timeout_ms < 0 ? 0 : read_timeout_ms;
    }
}

int implementation::add_string(const char *str, const char *attrs, int len, int x, int y, int textalign, int space) {
    if (!renderbuf) {
        logr_string->error("add_string w/o assimilated buffer");
        return 1;
    }

    const char *foo[] = { "left", "right", "center", "mono" };
    logr_string->trace("\"%s\" xy=%d,%d align=%s space=%d", str, x, y, foo[textalign], space);

    /* now space is always >1 and is in fact the space where the string must fit. */
    if (renderer->ttf_active() && (textalign != DF_MONOSPACE_LEFT)) {
        shrink::unicode shrinker(str, attrs, len);
        int grid_w;
        uint32_t w, h, ox, oy;

        grid_w = renderer->ttf_gridwidth(shrinker.chars(), shrinker.size(), &w, &h, &ox, &oy);
#if 0
        while (grid_w > space) {
            shrinker.shrink(shrinker.size() - 1);
            grid_w = renderer->ttf_gridwidth(shrinker.chars(), shrinker.size(), &w, &h, &ox, &oy);
        }
#endif

        logr_string->trace("grid_w = %d", grid_w);
        /* for now do alignment in terms of grid cells. pixels and the tab hack will go next */
        int grid_offset = 0;
        switch(textalign) {
        case DF_TEXTALIGN_CENTER:
            grid_offset = (len - grid_w)/2;
            break;
        case DF_TEXTALIGN_RIGHT:
            grid_offset = len - grid_w;
            break;
        default:
            break;
        }

        df_text_t *buftext = (df_text_t *) renderbuf->text;

        buftext->add_string(x + grid_offset, y, shrinker.chars(), shrinker.attrs(), shrinker.size(), w, h, ox, oy);
        logr_string->trace("gw=%d go=%d xy=%d,%d len %d returning %d", grid_w, grid_offset,
            x+grid_offset, y, shrinker.size(), grid_w + grid_offset);
        return grid_w + grid_offset;
        //return shrinker.size();
    } else {
        if ((space == 0) || (space >= len))
            return bputs_attrs(renderbuf, x, y, len, str, attrs);

        shrink::codepage shrinker(str, attrs, len);
        shrinker.shrink(space);
        return bputs_attrs(renderbuf, x, y, shrinker.size(), shrinker.chars(), shrinker.attrs());
    }
}

#if 0
int implementation::add_stringlist() {

/* TODO: get rid of the following */
typedef std::pair<std::string, unsigned> to_be_continued_t;
static std::list<to_be_continued_t> previously;
    /* replicate the tab-hack */
    {
        if (str.size() > 2 && str[0] == ':' && str[1] == ' ')
            str[1] = '\t'; // EVIL HACK

        to_be_continued_t tmp_tab;
        tmp_tab.second = 0x80u; // since MSB is not used otherwise.
        char *p = str_orig.c_str();
        char *q = p;
        while ((p = strchr(p, '\t'))) {
            std::string tmp_str(q, q-p);
            p++;
            q = p;
            to_be_continued_t tmp(tmp_str, attr);
            previously.push_back(tmp);
            previously.push_back(tmp_tab);
        }
        std::string tmp_str(q);
        to_be_continued_t tmp(tmp_str, attr);
        previously.push_back(tmp);
    }
}
#endif

void implementation::release() { }

static implementation *impl = NULL;

extern "C" DFM_EXPORT isimuloop * DFM_APIEP getsimuloop(void) {
    if (!impl) {
        _get_deps();
        simulogger = platform->getlogr("cc.simuloop");
        impl = new implementation();
        impl->logr_fps = platform->getlogr("cc.simuloop.fps");

    }
    return impl;
}
} /* ns */
