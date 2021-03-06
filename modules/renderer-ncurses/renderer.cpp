#include <cstdlib>
#include <map>

#include "ncurses.h"
#include "iplatform.h"
#include "imqueue.h"
#include "isimuloop.h"
#include "df_buffer.h"

#define DFMODULE_BUILD
#include "irenderer.h"

extern const unsigned codepage437[437];

iplatform *platform = NULL;
getplatform_t _getplatform = NULL;

namespace {

imqueue   *mqueue   = NULL;
isimuloop *simuloop = NULL;

struct implementation : public irenderer {
    void release(void);

    void zoom_in();
    void zoom_out();
    void zoom_reset();
    void toggle_fullscreen();
    void override_grid_size(unsigned, unsigned);
    void release_grid_size();
    int mouse_state(int *mx, int *my);

    void set_gridsize(unsigned w, unsigned h) { dimx = w, dimy = h; }
    uint32_t get_gridsize(void) { return (dimx << 16) | (dimy & 0xFFFF); }

    df_buffer_t *get_buffer();
    void submit_buffer(df_buffer_t *buf);
    df_buffer_t *get_offscreen_buffer(unsigned w, unsigned h);
    void export_offscreen_buffer(df_buffer_t *buf, const char *name);
    void reset_textures();

    void acknowledge(const itc_message_t&) {}

    int ttf_active();
    int ttf_gridwidth(const uint16_t*, uint32_t, uint32_t*, uint32_t*, uint32_t*,
                                                            uint32_t*, uint32_t*);

    void start();
    void join();
    void run_here();
    void simuloop_quit() { not_done = false; }
    bool started;

    ilogger *logr;

    imqd_t incoming_q;

    implementation();

    unsigned dimx, dimy;
    unsigned dropped_frames;
    bool not_done;
    void renderer_thread();
    thread_t thread_id;
    void slurp_keys();
};

implementation::implementation() {
    started = false;
    not_done = true;
    dropped_frames = 0;
    logr = platform->getlogr("ncurses");
    incoming_q = mqueue->open("renderer", 1<<10);

    /* don't care what init.txt says */
    int x, y;
    getmaxyx(stdscr, y, x);
    this->set_gridsize(x, y);
}
#define NOT_IMPLEMENTED(foo) logr->error("renderer_ncurses::" #foo " not implemented.\n")
void implementation::reset_textures() { NOT_IMPLEMENTED(reset_textures); }
void implementation::zoom_in() { NOT_IMPLEMENTED(zoom_in); }
void implementation::zoom_out() { NOT_IMPLEMENTED(zoom_out); }
void implementation::zoom_reset() { NOT_IMPLEMENTED(zoom_reset); }
void implementation::toggle_fullscreen() { NOT_IMPLEMENTED(toggle_fullscreen); }
void implementation::override_grid_size(unsigned, unsigned) { NOT_IMPLEMENTED(override_grid_size); }
void implementation::release_grid_size() { NOT_IMPLEMENTED(release_grid_size); }
int implementation::mouse_state(int *mx, int *my) { return *mx = 0, *my = 0, 0; }

int implementation::ttf_active() { return 0; }
int implementation::ttf_gridwidth(const uint16_t*, uint32_t a, uint32_t*, uint32_t*, uint32_t*,
                                                                          uint32_t*, uint32_t*) {
    NOT_IMPLEMENTED(ttf_width);
    return a;
}

void implementation::export_offscreen_buffer(df_buffer_t *buf, const char *name) {
    logr->info("exporting buf %p to %s", buf, name);
    free_buffer_t(buf);
}

df_buffer_t *implementation::get_offscreen_buffer(unsigned w, unsigned h) {
    return allocate_buffer_t(w, h, 0, 3);
}

/*  This is called from the simulation thread.
    Since this is a simplest possible implementation,
    just malloc it, set up pointers and be verbose about it.

    Since grid size is dictated by the renderer to the mainloop,
    read dimx and dimy across the thread from the backend.
    To make this atomic, pack them up into a single uint32_t. */
df_buffer_t *implementation::get_buffer(void) {
    uint32_t sizeval = this->get_gridsize();
    unsigned w = (sizeval >> 16) & 0xFFFF;
    unsigned h = sizeval & 0xFFFF;
    return allocate_buffer_t(w, h, 0, 3);
}

/*  This is called from the simulation thread.
    Since this is a simplest possible implementation,
    just add the buffer to the internal render queue. */
void implementation::submit_buffer(df_buffer_t *buf) {
    itc_message_t msg;
    msg.t = itc_message_t::render_buffer;
    msg.d.buffer = buf;
    mqueue->copy(incoming_q, &msg, sizeof(itc_message_t), -1);
}

/* stub for iplatform::create_thread() */
static int thread_stub(void *data) {
    implementation *owner = (implementation *) data;
    owner->renderer_thread();
    return 0;
}

void implementation::run_here() {
    renderer_thread();
}

void implementation::start() {
    if (started) {
        logr->error("second renderer start ignored\n");
        return;
    }
    started = true;
    thread_id = platform->thread_create(thread_stub, "renderer", (void *)this);
}

void implementation::join() {
    if (!started) {
        logr->error("irenderer::join(): not started\n");
        return;
    }
    platform->thread_join(thread_id, NULL);
}

// Map from DF color to ncurses color
static int ncurses_map_color(int color) {
    if (color < 0)
        getplatform()->fatal("fatal: ncurses_map_color(%d)\n", color);

    switch (color) {
        case 0: return 0;
        case 1: return 4;
        case 2: return 2;
        case 3: return 6;
        case 4: return 1;
        case 5: return 5;
        case 6: return 3;
        case 7: return 7;
        default: return ncurses_map_color(color - 7);
    }
}

std::map<std::pair<int,int>,int> color_pairs;

// Look up, or create, a curses color pair
int lookup_pair(std::pair<int,int> color) {
    std::map<std::pair<int,int>,int>::iterator it = color_pairs.find(color);

    if (it != color_pairs.end())
        return it->second;

    // We don't already have it. Make sure it's in range.
    if (color.first < 0 || color.first > 7 || color.second < 0 || color.second > 7)
        return 0;

    // We don't already have it. Generate a new pair if possible.
    if (color_pairs.size() + 1 < ((unsigned)COLOR_PAIRS)) {
        const short pair = color_pairs.size() + 1;
        init_pair(pair, ncurses_map_color(color.first), ncurses_map_color(color.second));
        color_pairs[color] = pair;
        return pair;
    }

    // We don't have it, and there's no space for more. Find the closest equivalent.
    int score = 999, pair = 0;
    int rfg = color.first % 16, rbg = color.second % 16;
    for (auto it = color_pairs.cbegin(); it != color_pairs.cend(); ++it) {
        int fg = it->first.first;
        int bg = it->first.second;
        int candidate = it->second;
        int candidate_score = 0;  // Lower is better.

        if (rbg != bg) {
            if (rbg == 0 || rbg == 15)
                candidate_score += 3;  // We would like to keep the background black/white.

            if ((rbg == 7 || rbg == 8)) {
                if (bg == 7 || bg == 8)
                    candidate_score += 1; // Well, it's still grey.
                else
                    candidate_score += 2;
            }
        }

        if (rfg != fg) {
            if (rfg == 0 || rfg == 15)
                candidate_score += 5; // Keep the foreground black/white if at all possible.
            if (rfg == 7 || rfg == 8) {
                if (fg == 7 || fg == 8)
                    candidate_score += 1; // Still grey. Meh.
                else
                    candidate_score += 3;
            }
        }
        if (candidate_score < score)
            score = candidate_score, pair = candidate;
    }
    color_pairs[color] = pair;
    return pair;
}

void implementation::slurp_keys() {
    /* slurp all input there is and stuff it into
       simuloop's message queue. Don't pause simuloop,
       and absolutely don't wait until it gets paused.

       TODO:
            - shifted arrow keys, etc. See man 5 terminfo and tgetent.
            - mouse events */

    df_input_event_t event;
    uint32_t now = platform->GetTickCount();

    do {
        event.type = df_input_event_t::DF_KEY_DOWN;
        event.now = now;
        event.mod = DFMOD_NONE;
        event.reports_release = false;
        event.sym = DFKS_UNKNOWN;
        event.unicode = 0;

        wint_t key;
        int what = get_wch(&key);
#if defined(DEBUG_INPUT)
        if (what != -1)
            logr->info("get_wch(): what=%d key=%u.", what, key);
#endif
        switch (what) {
            case ERR:
                break;

            case OK:
                switch (key) {
                    /* synthesize sym */
                    case   9: event.sym = DFKS_TAB; break;
                    case  10: event.sym = DFKS_RETURN; break;
                    case 127: event.sym = DFKS_BACKSPACE; break;
                    case  27: event.sym = DFKS_ESCAPE; break;
                    default:
                        if (key > 0 && key <= 26) { // Control-a through z (but not ctrl-j, or ctrl-i)
                            event.mod |= DFMOD_CTRL;
                            event.sym = (DFKeySym)(DFKS_a + key - 1);
                            break;
                        }
                        if (key >= 32 && key <= 126) { // ASCII character set
                            event.unicode = key;
                            event.sym = (DFKeySym)+key; // Most of this maps directly to DF keys, except..
                            if (event.sym > 64 && event.sym < 91) { // Uppercase
                                event.sym = (DFKeySym)(event.sym + 32); // Maps to lowercase, and
                                event.mod |= DFMOD_SHIFT; // Add shift.
                            }
                        }
                        break;
                }
                event.unicode = key;
                simuloop->add_input_event(&event);
                continue;

            case KEY_CODE_YES:
                switch (key) {
                    case KEY_DOWN:      event.sym = DFKS_DOWN; break;
                    case KEY_UP:        event.sym = DFKS_UP; break;
                    case KEY_LEFT:      event.sym = DFKS_LEFT; break;
                    case KEY_RIGHT:     event.sym = DFKS_RIGHT; break;
                    case KEY_BACKSPACE: event.sym = DFKS_BACKSPACE; break;
                    case KEY_F(1):      event.sym = DFKS_F1; break;
                    case KEY_F(2):      event.sym = DFKS_F2; break;
                    case KEY_F(3):      event.sym = DFKS_F3; break;
                    case KEY_F(4):      event.sym = DFKS_F4; break;
                    case KEY_F(5):      event.sym = DFKS_F5; break;
                    case KEY_F(6):      event.sym = DFKS_F6; break;
                    case KEY_F(7):      event.sym = DFKS_F7; break;
                    case KEY_F(8):      event.sym = DFKS_F8; break;
                    case KEY_F(9):      event.sym = DFKS_F9; break;
                    case KEY_F(10):     event.sym = DFKS_F10; break;
                    case KEY_F(11):     event.sym = DFKS_F11; break;
                    case KEY_F(12):     event.sym = DFKS_F12; break;
                    case KEY_F(13):     event.sym = DFKS_F13; break;
                    case KEY_F(14):     event.sym = DFKS_F14; break;
                    case KEY_F(15):     event.sym = DFKS_F15; break;
                    case KEY_DC:        event.sym = DFKS_DELETE; break;
                    case KEY_NPAGE:     event.sym = DFKS_PAGEDOWN; break;
                    case KEY_PPAGE:     event.sym = DFKS_PAGEUP; break;
                    case KEY_ENTER:     event.sym = DFKS_RETURN; break;
                    default:
                        logr->info("get_wch(): what=%d key=%u, skipped.", what, key);
                        continue;
                }
                simuloop->add_input_event(&event);
                continue;
            default:
                logr->info("get_wch(): what=%d key=%u, skipped.", what, key);
                continue;
        }
    } while(false);
}

void implementation::renderer_thread(void) {
    /* pseudocode:
        - see if simulation thread died; exit if so.
        - get a frame from the incoming_q
        - render it, observing g_fps clamp
        - look for input
        - send any input to the simulation thread
        - rejoice
    */

    while(not_done) {
        int x, y;
        getmaxyx(stdscr, y, x);
        this->set_gridsize(x, y);

        slurp_keys();

        /* wait for a buffer to render or simuloop to quit */
        /* todo: drop any extra frames, render only the last one. */
        df_buffer_t *buf = NULL;
        unsigned read_timeout_ms = 10; // this clamps gfps to 100.
        while (true) {
            void *vbuf; size_t vlen;
            int rv = mqueue->recv(incoming_q, &vbuf, &vlen, read_timeout_ms);
            read_timeout_ms = 0; // reset timeout so we don't wait second time.

            if (rv == IMQ_TIMEDOUT)
                break;

            if (rv != 0)
                platform->fatal("render_thread(): %d from mqueue->recv().");

            itc_message_t *msg = (itc_message_t *)vbuf;

            switch (msg->t) {
                case itc_message_t::render_buffer:
                    if (buf) {
                        dropped_frames ++;
                        free_buffer_t(buf);
                    }
                    buf = msg->d.buffer;
                    mqueue->free(msg);
                    break;
                default:
                    logr->error("render_thread(): unknown message type %d", msg->t);
                    break;
            }
        }
        if (buf) {
            for (uint32_t x = 0; x < buf->w; x++)
                for (uint32_t y = 0; y < buf->h; y++) {
                    const int ch   = buf->screen[x*buf->h*4 + y*4 + 0];
                    const int fg   = buf->screen[x*buf->h*4 + y*4 + 1];
                    const int bg   = buf->screen[x*buf->h*4 + y*4 + 2];
                    const int bold = buf->screen[x*buf->h*4 + y*4 + 3];

                    const int pair = lookup_pair(std::make_pair(fg, bg));

                    if ((ch == 219) && !bold) {
                        // It's █, which is used for borders and digging designations.
                        // A_REVERSE space looks better if it isn't completely tall.
                        // Which is most of the time, for me at least.
                        // █ <-- Do you see gaps?
                        // █
                        // The color can't be bold.
                        wattrset(stdscr, COLOR_PAIR(pair) | A_REVERSE);
                        mvwaddstr(stdscr, y, x, " ");
                    } else {
                        wattrset(stdscr, COLOR_PAIR(pair) | (bold ? A_BOLD : 0));
                        wchar_t chs[2] = { codepage437[ch], 0 };
                        mvwaddwstr(stdscr, y, x, chs);
                    }
                }
            free_buffer_t(buf);
            refresh();
        }
    }
}

void implementation::release(void) { }

/* module glue */

static implementation *impl = NULL;

getmqueue_t   _getmqueue = NULL;
getsimuloop_t _getsimuloop = NULL;

extern "C" DFM_EXPORT void DFM_APIEP dependencies(
                            getplatform_t   **pl,
                            getmqueue_t     **mq,
                            getrenderer_t   **rr,
                            gettextures_t   **tx,
                            getsimuloop_t   **sl,
                            getmusicsound_t **ms,
                            getkeyboard_t   **kb) {
    *pl = &_getplatform;
    *mq = &_getmqueue;
    *rr = NULL;
    *tx = NULL;
    *sl = &_getsimuloop;
    *ms = NULL;
    *kb = NULL;
}

extern "C" DFM_EXPORT irenderer * DFM_APIEP getrenderer(void) {
    if (!impl) {
        platform = _getplatform();
        mqueue   = _getmqueue();
        simuloop = _getsimuloop();
        impl = new implementation();
    }
    return impl;
}
} /* ns */
