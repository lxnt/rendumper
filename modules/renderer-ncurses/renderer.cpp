#include <cstdlib>
#include <map>

#include "ncurses.h"
#include "iplatform.h"
#include "imqueue.h"
#include "isimuloop.h"

#define DFMODULE_BUILD
#include "irenderer.h"

namespace {

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

    void start();
    void join();

    bool started;

    iplatform *platform;
    imqueue *mqueue;
    isimuloop *simuloop;

    imqd_t buffer_q;
    imqd_t request_q;

    implementation();

    unsigned dimx, dimy;
    void renderer_thread();
    thread_t thread_id;
};

implementation::implementation() {
    started = false;
    platform = getplatform();
    simuloop = getsimuloop();
    mqueue = getmqueue();
    buffer_q = mqueue->open("buffer_q", 1<<10);
    request_q = mqueue->open("request_q", 1<<10);

    /* don't care what init.txt says */
    int x, y;
    getmaxyx(stdscr, y, x);
    this->set_gridsize(x, y);
}
#define NOT_IMPLEMENTED(foo) getplatform()->log_error("renderer_ncurses::" #foo " not implemented.\n")
void implementation::zoom_in() { NOT_IMPLEMENTED(zoom_in); }
void implementation::zoom_out() { NOT_IMPLEMENTED(zoom_out); }
void implementation::zoom_reset() { NOT_IMPLEMENTED(zoom_reset); }
void implementation::toggle_fullscreen() { NOT_IMPLEMENTED(toggle_fullscreen); }
void implementation::override_grid_size(unsigned, unsigned) { NOT_IMPLEMENTED(override_grid_size); }
void implementation::release_grid_size() { NOT_IMPLEMENTED(release_grid_size); }
int implementation::mouse_state(int *mx, int *my) { return *mx = 0, *my = 0, 0; }

static inline size_t pot_align(size_t value, unsigned pot) {
    return (value + (1<<pot) - 1) & ~((1<<pot) -1);
}

/*  This is called from the simulation thread.
    Since this is a simplest possible implementation,
    just malloc it, set up pointers and be verbose about it.

    Since grid size is dictated by the renderer to the mainloop,
    read dimx and dimy across the thread from the backend.
    To make this atomic, pack them up into a single uint32_t.

    Excersize 64bit alignment, because we can. */
df_buffer_t *implementation::get_buffer(void) {
    uint32_t sizeval = this->get_gridsize();
    unsigned dimx = (sizeval >> 16) & 0xFFFF;
    unsigned dimy = sizeval & 0xFFFF;

    const unsigned pot = 3;  /* 1<<3 = 8 bytes; 64 bits */
    const size_t screen_sz    = pot_align(dimx*dimy*4, pot);
    const size_t texpos_sz    = pot_align(dimx*dimy*sizeof(long), pot);
    const size_t addcolor_sz  = pot_align(dimx*dimy, pot);
    const size_t grayscale_sz = pot_align(dimx*dimy, pot);
    const size_t cf_sz        = pot_align(dimx*dimy, pot);
    const size_t cbr_sz       = pot_align(dimx*dimy, pot);

    df_buffer_t *rv = (df_buffer_t *) malloc(sizeof(df_buffer_t));

    /*  malloc(3) says:
            The malloc() and calloc() functions return a pointer
            to the allocated memory that is suitably aligned for
            any  kind  of variable.

        memalign(3) says:
            The  glibc  malloc(3) always returns 8-byte aligned
            memory addresses, so these functions are only needed
            if you require larger alignment values. */

    rv->screen = (unsigned char *) malloc(screen_sz + texpos_sz + addcolor_sz
        + grayscale_sz + cf_sz + cbr_sz);
    rv->texpos = (long *) (rv->screen + screen_sz);
    rv->addcolor = (char *) rv->screen + screen_sz + texpos_sz;
    rv->grayscale = rv->screen + screen_sz + texpos_sz + addcolor_sz;
    rv->cf = rv->screen + screen_sz + texpos_sz + addcolor_sz + grayscale_sz;
    rv->cbr = rv->screen + screen_sz + texpos_sz + addcolor_sz + grayscale_sz + cf_sz;

    rv->dimx = dimx, rv->dimy = dimy;

    return rv;
}

/*  This is called from the simulation thread.
    Since this is a simplest possible implementation,
    just add the buffer to the internal render queue. */
void implementation::submit_buffer(df_buffer_t *buf) {
    mqueue->send(buffer_q, buf, sizeof(df_buffer_t), -1);
}

/* stub for iplatform::create_thread() */
static int thread_stub(void *data) {
    implementation *owner = (implementation *) data;
    owner->renderer_thread();
    return 0;
}

void implementation::start() {
    if (started) {
        platform->log_error("second renderer start ignored\n");
        return;
    }
    started = true;
    thread_id = platform->thread_create(thread_stub, "renderer", (void *)this);
}

void implementation::join() {
    if (!started) {
        platform->log_error("irenderer::join(): not started\n");
        return;
    }
    platform->thread_join(thread_id, NULL);
}

// Map from DF color to ncurses color
static int ncurses_map_color(int color) {
    if (color < 0) {
        getplatform()->log_error("fatal: ncurses_map_color(%d)\n", color);
        abort();
    }
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

// Map DF's CP437 to Unicode
// see: http://dwarffortresswiki.net/index.php/Character_table
static const int charmap[256] = {
    0x20, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022,
    0x25D8, 0x25CB, 0x25D9, 0x2642, 0x2640, 0x266A, 0x266B, 0x263C,
    0x25BA, 0x25C4, 0x2195, 0x203C, 0x00B6, 0x00A7, 0x25AC, 0x21A8,
    0x2191, 0x2193, 0x2192, 0x2190, 0x221F, 0x2194, 0x25B2, 0x25BC,
    /* 0x20 */
    0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
    0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
    0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,
    0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
    0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x2302,
    /* 0x80 */
    0xC7, 0xFC, 0xE9, 0xE2, 0xE4, 0xE0, 0xE5, 0xE7,
    0xEA, 0xEB, 0xE8, 0xEF, 0xEE, 0xEC, 0xC4, 0xC5,
    0xC9, 0xE6, 0xC6, 0xF4, 0xF6, 0xF2, 0xFB, 0xF9,
    0xFF, 0xD6, 0xDC, 0xA2, 0xA3, 0xA5, 0x20A7, 0x192,
    0xE1, 0xED, 0xF3, 0xFA, 0xF1, 0xD1, 0xAA, 0xBA,
    0xBF, 0x2310, 0xAC, 0xBD, 0xBC, 0xA1, 0xAB, 0xBB,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
    0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
    0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F,
    0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
    0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B,
    0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
    0x3B1, 0xDF/*yay*/, 0x393, 0x3C0, 0x3A3, 0x3C3, 0xB5, 0x3C4,
    0x3A6, 0x398, 0x3A9, 0x3B4, 0x221E, 0x3C6, 0x3B5, 0x2229,
    0x2261, 0xB1, 0x2265, 0x2264, 0x2320, 0x2321, 0xF7, 0x2248,
    0xB0, 0x2219, 0xB7, 0x221A, 0x207F, 0xB2, 0x25A0, 0xA0
};

void implementation::renderer_thread(void) {
    /* pseudocode:
        - see if simulation thread died; exit if so.
        - get a frame from the buffer_q
        - render it, observing g_fps clamp
        - look for input
        - send any input to the simulation thread
        - rejoice
    */

    while(true) {
        int x, y;
        getmaxyx(stdscr, y, x);
        this->set_gridsize(x, y);

        /* slurp all input there is and stuff it into
           simuloop's message queue. Don't pause simuloop,
           and absolutely don't wait until it gets paused. */
        uint32_t now = platform->GetTickCount();
        do {
            int what;
            wint_t key;

            switch (what = get_wch(&key)) {
                case ERR:
                    break;
                case KEY_CODE_YES:
                    simuloop->add_input_ncurses(key, now);
                    continue;
                default:
                    platform->log_info("get_wch(): what=%d key=%u\n", what, key);
                    simuloop->add_input_ncurses(key, now);
                    continue;
            }
        } while(false);

        /* wait for a buffer to render */
        void *vbuf; size_t vlen;
        int rv = mqueue->recv(buffer_q, &vbuf, &vlen, -1);

        if (rv == IMQ_TIMEDOUT)
            continue;

        if (rv == 0) {
            df_buffer_t *buf = (df_buffer_t *)vbuf;

            for (int x = 0; x < buf->dimx; x++)
                for (int y = 0; y < buf->dimy; y++) {
                    const int ch   = buf->screen[x*buf->dimy*4 + y*4 + 0];
                    const int fg   = buf->screen[x*buf->dimy*4 + y*4 + 1];
                    const int bg   = buf->screen[x*buf->dimy*4 + y*4 + 2];
                    const int bold = buf->screen[x*buf->dimy*4 + y*4 + 3];

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
                        wchar_t chs[2] = { charmap[ch], 0 };
                        mvwaddwstr(stdscr, y, x, chs);
                    }
                }

            refresh();

        } else {
            platform->log_error("renderer: %d from mq->recv(bufq)\n", rv);
        }
    }
}

void implementation::release(void) { }

static implementation *impl = NULL;
extern "C" DECLSPEC irenderer * APIENTRY getrenderer(void) {
    if (!impl)
        impl = new implementation();
    return impl;
}
} /* ns */
