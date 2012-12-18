/* this is the testing code for the libgraphics interfaces.
   As such it is a bare-bones implementation of the libgraphics.so
   sans everything that got chopped off into the modules.

   to make the test closer to the intended functionality,
   mutable conway's game of life is hereby implemented.

*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include "iplatform.h"
#include "irenderer.h"
#include "isimuloop.h"
#include "itypes.h"
#include "itextures.h"


#include "glue.h"

//{ guts

iplatform *platform = NULL;

struct lifeform_t {
    int w, h;
    uint8_t cells[4096];
};

const lifeform_t blob  = { 2, 2, "xxxx" };

const lifeform_t toad = { 4, 2,
    " xxx"
    "xxx " };

const lifeform_t glider = { 3, 3,
    "x  "
    " xx"
    "xxx" };

const lifeform_t ggg = {  36, 9,
    "                        x           "
    "                      x x           "
    "            xx      xx            xx"
    "           x   x    xx            xx"
    "xx        x     x   xx              "
    "xx        x   x xx    x x           "
    "          x     x       x           "
    "           x   x                    "
    "            xx                      " };

const lifeform_t blse { 10, 6,
    "       x "
    "     x xx"
    "     x x "
    "     x   "
    "  x      "
    "x x      " };

const lifeform_t twentyfive { 5, 5,
    "xxx x"
    "x    "
    "   xx"
    " xx x"
    "x x x" };

struct lfidxntry { const char name[32]; const lifeform_t form; } forms[] = {
    { "blob", blob },
    { "toad", toad },
    { "ggg",   ggg },
    { "blse", blse },
    { "twentyfive", twentyfive}
};

char c64screen[1001] =
    "                                        "
    "    **** COMMODORE 64 BASIC V2 ****     "
    "                                        "
    " 64K RAM SYSTEM  38911 BASIC BYTES FREE "
    "                                        "
    "READY.                                  "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        ";

struct c64frame {
    uint32_t x,y,w,h;
    uint8_t frame[256];

} c64load[] = {
    { 0, 0,  0, 0, "" },
    { 0, 6,  2, 1, "L\xdb" },
    { 0, 6,  3, 1, "LO\xdb" },
    { 0, 6,  4, 1, "LOA\xdb" },
    { 0, 6,  5, 1, "LOAD\xdb" },
    { 0, 6,  6, 1, "LOAD \xdb" },
    { 0, 6,  7, 1, "LOAD \"\xdb" },
    { 0, 6,  8, 1, "LOAD \"L\xdb" },
    { 0, 6,  9, 1, "LOAD \"LI\xdb" },
    { 0, 6, 10, 1, "LOAD \"LIF\xdb" },
    { 0, 6, 11, 1, "LOAD \"LIFE\xdb" },
    { 0, 6, 12, 1, "LOAD \"LIFE\"\xdb" },
    { 0, 6, 13, 1, "LOAD \"LIFE\",\xdb" },
    { 0, 6, 14, 1, "LOAD \"LIFE\",8\xdb" },
    { 0, 6, 15, 1, "LOAD \"LIFE\",8,\xdb" },
    { 0, 6, 16, 1, "LOAD \"LIFE\",8,1\xdb" },
    { 0, 6, 16, 1, "LOAD \"LIFE\",8,1 " },
    { 0, 6, 18, 2, "LOAD \"LIFE\",8,1   "
                   "SEARCHING FOR LIFE" },
    { 0, 6, 18, 2, "LOAD \"LIFE\",8,1   "
                   "SEARCHING FOR LIFE" },
    { 0, 6, 18, 2, "LOAD \"LIFE\",8,1   "
                   "SEARCHING FOR LIFE" },
    { 0, 6, 18, 3, "LOAD \"LIFE\",8,1   "
                   "SEARCHING FOR LIFE"
                   "LOADING           " },
    { 0, 6, 18, 3, "LOAD \"LIFE\",8,1   "
                   "SEARCHING FOR LIFE"
                   "LOADING           " },
    { 0, 6, 18, 3, "LOAD \"LIFE\",8,1   "
                   "SEARCHING FOR LIFE"
                   "LOADING           " },
    { 0, 6, 18, 3, "LOAD \"LIFE\",8,1   "
                   "SEARCHING FOR LIFE"
                   "LOADING           " },
    { 0, 6, 18, 3, "LOAD \"LIFE\",8,1   "
                   "SEARCHING FOR LIFE"
                   "LOADING           " },
    { 0, 6, 18, 3, "LOAD \"LIFE\",8,1   "
                   "SEARCHING FOR LIFE"
                   "LOADING           " },
    { 0, 6, 18, 5, "LOAD \"LIFE\",8,1   "
                   "SEARCHING FOR LIFE"
                   "LOADING           "
                   "READY.            "
                   "\xdb                 " },
    { 0, 6, 18, 5, "LOAD \"LIFE\",8,1   "
                   "SEARCHING FOR LIFE"
                   "LOADING           "
                   "READY.            "
                   "                  " },
    { 0, 6, 18, 5, "LOAD \"LIFE\",8,1   "
                   "SEARCHING FOR LIFE"
                   "LOADING           "
                   "READY.            "
                   "\xdb                 " },
    { 0, 6, 18, 5, "LOAD \"LIFE\",8,1   "
                   "SEARCHING FOR LIFE"
                   "LOADING           "
                   "READY.            "
                   "\xdb                 " },
    { 0, 6, 18, 5, "LOAD \"LIFE\",8,1   "
                   "SEARCHING FOR LIFE"
                   "LOADING           "
                   "READY.            "
                   "R\xdb                " },
    { 0, 6, 18, 5, "LOAD \"LIFE\",8,1   "
                   "SEARCHING FOR LIFE"
                   "LOADING           "
                   "READY.            "
                   "RU\xdb               " },
    { 0, 6, 18, 5, "LOAD \"LIFE\",8,1   "
                   "SEARCHING FOR LIFE"
                   "LOADING           "
                   "READY.            "
                   "RUN\xdb              " },
    { 0, 6, 18, 5, "LOAD \"LIFE\",8,1   "
                   "SEARCHING FOR LIFE"
                   "LOADING           "
                   "READY.            "
                   "RUN               " }
                   };


struct gofsim_t {
    bool tor;
    int *field;
    int *dbuf;
    int w, h;

    gofsim_t(int _w, int _h, bool toroidal) {
        tor = toroidal;
        w = _w; h = _h;
        field = (int *)calloc(w*h, sizeof(int));
        dbuf = (int *)calloc(w*h, sizeof(int));
    }
    ~gofsim_t() {
        free(field);
        free(dbuf);
    }

    int get(int x, int y) {
        if (x < 0) {
            if (tor)
                x = w + x;
            else
                return 0;
        } else if (x > w-1) {
            if (tor)
                x = x - (w-1);
            else
                return 0;
        }

        if (y < 0) {
            if (tor)
                y = h + y;
            else
                return 0;
        } else if (y > h-1) {
            if (tor)
                y = y - (h-1);
            else
                return 0;
        }

        return *(field + x + y*w);
    }

    void set(int x, int y, int v) {
        *(dbuf + x + y*w) = v;
    }

    /* divine intervention */
    void di_set(int x, int y, int v) {
        *(field + x +y*w) = v;
    }

    void tick(void) {
        int neigh[8];
        int dead = 0, born = 0;
        memset(dbuf, 0, sizeof(int)*w*h);
        for (int x = 0; x < w ; x++) {
            for(int y = 0; y < h ; y++) {
                int n = 0;
                if ((neigh[n] = get(x-1, y-1)))
                    n++;
                if ((neigh[n] = get(x+1, y-1)))
                    n++;
                if ((neigh[n] = get(x-1, y+1)))
                    n++;
                if ((neigh[n] = get(x+1, y+1)))
                    n++;
                if ((neigh[n] = get(x-1, y )))
                    n++;
                if ((neigh[n] = get(x,   y-1)))
                    n++;
                if ((neigh[n] = get(x+1, y)))
                    n++;
                if ((neigh[n] = get(x,   y+1)))
                    n++;
                if (get(x,y)) {
                    if  (((n!=3) && (n!=2))) {
                        set(x,y,0);
                        dead ++;
                    } else {
                        set(x,y,get(x,y));
                    }
                    continue;
                } else {
                    if (n == 3) {
                        born ++;
                        // QuadLife
                        if ((neigh[0] != neigh[1])
                            && (neigh[0] != neigh[2])
                            && (neigh[1] != neigh[2])) {
                            for (int other = 1; other < 5 ; other ++ ) {
                                if (    (other != neigh[0])
                                     && (other != neigh[1])
                                     && (other != neigh[2]) ) {
                                    set(x, y, other);
                                    break;
                                }
                            }
                        } else {
                            if ((neigh[0] == neigh[1]) || (neigh[0] == neigh[2])) {
                                set(x, y, neigh[0]);
                            } else {
                                set(x, y, neigh[1]);
                            }
                        }
                    }
                }
            }
        }
        int *tmp = field;
        field = dbuf;
        dbuf = tmp;
    }

    int count() {
        int rv = 0;
        for(int i = 0; i < w*h; i++)
            if (*(field + i))
                rv++;
        return rv;
    }

    void paste(int x, int y, const lifeform_t& form, int n) {
        int xl = form.w < w - x ? form.w : w - x;
        int yl = form.h < h - y ? form.h : h - y;
        for (int xt = 0; xt < xl; xt++)
            for (int yt = 0; yt < yl; yt++)
                di_set(x+xt, y+yt, (form.cells[xt + yt*form.w] != 32) ? n : 0);
    }

};

struct gofui_t {
    int vp_x, vp_y;

    gofui_t() {
        vp_x = vp_y = 0;
        quit = single_step = false;
        paused = first_pause = true;
        unpaused_at = 0;
        unpause_seq = 0;
        targetbuf = NULL;
    }

    df_buffer_t *targetbuf;
    bool quit;
    bool paused;
    bool first_pause;
    bool single_step;
    uint32_t unpaused_at, unpause_seq;

    void add_input_event(df_input_event_t *event) {
        if ((event->type == df_input_event_t::DF_QUIT) || (event->sym == DFKS_ESCAPE))
            quit = true;
        else if (event->type == df_input_event_t::DF_KEY_DOWN) {

            if (!first_pause and !unpause_seq and event->sym == '.') {
                if (paused)
                    single_step = true;
                return;
            }

            /* rest of keys pause/unpause/skipintro */
            if (first_pause) {
                first_pause = false;
                if (!unpaused_at) {
                    unpaused_at = platform->GetTickCount();
                    unpause_seq = 1;
                }
            } else {
                if (!unpause_seq)
                    paused = not paused;
                else
                    unpause_seq = 0; // skip intro
            }
        }
    }

    void assimilate_buffer(df_buffer_t *buffer) { targetbuf = buffer; }
    void render_things();
};

gofsim_t *sim;
gofui_t *ui;

char mainloop(void) {
    if (!ui->paused)
        sim->tick();
    else if (ui->single_step) {
        sim->tick();
        ui->single_step = false;
    }

    return ui->quit;
}
void add_input_event(df_input_event_t *event) { ui->add_input_event(event); }
void assimilate_buffer(df_buffer_t *buf) { ui->assimilate_buffer(buf); }
void render_things() { ui->render_things(); }

#if defined(WIN32)
/* this so needs to be moved into the platform code */
/* http://msdn.microsoft.com/en-us/library/a9yf33zb%28v=vs.80%29.aspx */
void iph_stub(const wchar_t * /* expr */,
              const wchar_t * /* function */,
              const wchar_t * /* file */,
              unsigned int  /* line */,
              uintptr_t) { }

void windoze_crap() {
    _set_invalid_parameter_handler(iph_stub);
    _CrtSetReportMode(_CRT_ASSERT, 0);
}
#define vsnprintf _vsnprintf
#endif

/*
    puts a string into the buffer. string gets truncated at size characters
    or at the buffer border; no wrapping.

    attrs are:
        lil endian:
            (br<<24) | (bg<<16) | (fg<<8)
        big endian:
            (fg<<16) | (bg<<8 )| (br)

    return value:
        characters written, that is min(final_strlen, size, dimx-x)
        or -1.
*/
int snprintf(df_buffer_t *buffer, uint32_t x, uint32_t y, size_t size, uint32_t attrs, const char *fmt, ...) {
    char tbuf[size + 1];

    memset(tbuf, 0, size+1);

    va_list ap;
    va_start(ap, fmt);
    int rv = vsnprintf(tbuf, size + 1, fmt, ap);
    va_end(ap);

#if defined(WIN32)
    if (rv < 0) {
        if (strlen(tbuf) == size))
            rv = size;
        else
            return rv;
    }
#else
    if (rv < 0)
        return rv;
#endif
    uint32_t printed = rv;
    uint32_t i;
    uint32_t *ptr = (uint32_t *)(buffer->screen);

    for (i = 0 ; (i < buffer->w - x) && ( i < printed ); i++) {
        uint32_t offset = (x + i) * buffer->h + y;
        *( ptr + offset ) = attrs | tbuf[i];
    }
    return i;
}


void gofui_t::render_things(void) {
    df_buffer_t *buf = targetbuf;
    /* now render a portion of the map into the buffer
       according to its dimx/dimy
       this example assumes map cell values are indices
       into the texture list. this is not so in DF,
       and this mapping step, if necessary, is to be done here.

       the viewport position is to be kept where?? in the interface
       object, of course.
    */

    memset(buf->ptr, 0, buf->required_sz);

    for (uint32_t i = 0; i < buf->w; i++)
        for (uint32_t j = 0; j < buf->h; j++) {
            uint32_t index = ( buf->h * i + j );
            int value = sim->get( ( i + ui->vp_x ) % buf->w,
                                   ( j + ui->vp_y ) % buf->h );
            buf->screen[4*index] = 111; // 'o'
            buf->screen[4*index+1] = value; // fg, 1:1 map to ansi is nice
            buf->screen[4*index+2] = 0; // bg
            buf->screen[4*index+3] = 1; // br
            buf->texpos[index] = value;
        }

    snprintf(buf, buf->w/2, 0, 20, 0x01010100, "vp: %d,%d", ui->vp_x, ui->vp_y);
    snprintf(buf, 0, 0, 10, 0x01010100, ui->paused ? "[PAUSED]" : "[RUNNING]");


    if (first_pause || unpause_seq) {
        uint32_t *uibuf = (uint32_t *)(buf->screen);
        uint32_t xoffs = (buf->w - 40)/2;
        uint32_t yoffs = (buf->h > 25) ? (buf->h - 25)/2 : 0;
        for (uint32_t i = 0; i < buf->w*buf->h; i++)
            *(uibuf + i) = 0x010101db;
        for (uint32_t i = 0; i < 40*(yoffs ? 25 : buf->h); i++)
            *(uibuf + (i%40 + xoffs) * buf->h + i/40+yoffs) = 0x01010100 | c64screen[i];

        if (!unpause_seq) {
            *(uibuf + yoffs + 6 + buf->h*xoffs) = (platform->GetTickCount() % 1000 < 500) ? 0x010101db : 0x01010100;
        } else {
            for (uint32_t i = 0; i < c64load[unpause_seq].w * c64load[unpause_seq].h; i++) {
                uint32_t scr_x = (i % c64load[unpause_seq].w + c64load[unpause_seq].x) + xoffs;
                uint32_t scr_y = (i / c64load[unpause_seq].w + c64load[unpause_seq].y) + yoffs;
                if (( scr_x >= buf->w )||( scr_y >= buf->h ))
                    continue;

                *(uibuf + scr_x * buf->h + scr_y) = 0x01010100 | c64load[unpause_seq].frame[i];
            }
            unpause_seq = (platform->GetTickCount() - ui->unpaused_at)/200 + 1;
            if (unpause_seq > (sizeof(c64load) / sizeof(c64frame) ) - 1)
                unpause_seq = 0;
        }
    }
}

//}

int main (int argc, char* argv[]) {
#if defined(WIN32)
    windoze_crap();
#endif
    if (argc < 2) {
        fprintf(stderr, "Usage: %s print_mode [w h] [tor]\n", argv[0]);
        return 1;
    }

    if (!lock_and_load(argv[1], NULL))
        return 1;

    int w = 128, h = 32;
    bool toroidal = false;
    if (argc >= 5) {
        w = atoi(argv[3]);
        h = atoi(argv[4]);
    }
    if (argc == 6)
        toroidal = true;

    sim = new gofsim_t(w, h, toroidal);
    sim->paste(5, 5, toad, 2);
    sim->paste(10, 10, ggg, 1);

    ui = new gofui_t();

    // get platform - trigger initialization
    platform = getplatform();
    irenderer *renderer = getrenderer();
    isimuloop *simuloop = getsimuloop();

    // set up the simuloop
    simuloop->set_callbacks(mainloop, render_things, assimilate_buffer, add_input_event);
    simuloop->set_target_sfps(2);
    simuloop->set_target_rfps(4);

    // run the loops
    renderer->start();
    simuloop->start();
    renderer->join();

    delete sim;
    return 0;
}
