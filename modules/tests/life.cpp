/* this is the testing code for the libgraphics interfaces.
   As such it is a bare-bones implementation of the libgraphics.so
   sans everything that got chopped off into the modules.

   to make the test closer to the intended functionality,
   mutable conway's game of life is hereby implemented.

*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "iplatform.h"
#include "irenderer.h"
#include "isimuloop.h"
#include "ikeyboard.h"
#include "itextures.h"
#include "imusicsound.h"

#include "glue.h"

/* TBD:
    SDL_Init() and other such stuff to be handled on module load ?
        Resolved: SDL_Init() with default set of subsystems or
        an alternative gets called on plaform module load, which must
        be the first one to be loaded.

    RAII: getisimuloop() starts and pauses the simuloop thread.
          the simuloop can't be unpaused until the first (and only)
          call to the set_callbacks()
*/

struct gofsim_t {
    int *field;
    int *dbuf;
    int w, h;

    gofsim_t(int _w, int _h) {
        w = _w; h = _h;
        field = (int *)calloc(w*h, 1);
        dbuf = (int *)calloc(w*h, 1);
    }
    ~gofsim_t() {
        free(field);
        free(dbuf);
    }

    int get(int x, int y) {
        if (x<0)
            x = w+x;
        else if (x>w-1)
            x = x - (w-1);
        if (y<0)
            y = h+y;
        else if (y > h-1)
            y = y - (h-1);

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
        memset(dbuf, 0, sizeof(int)*w*h);
        for (int x = 0; x < w ; x++) {
            for(int y = 0; y < h ; y++) {
                int n = 0;
                if (neigh[n] = get(x-1, y-1))
                    n++;
                if (neigh[n] = get(x+1, y-1))
                    n++;
                if (neigh[n] = get(x-1, y+1))
                    n++;
                if (neigh[n] = get(x+1, y+1))
                    n++;
                if (neigh[n] = get(x-1, y ))
                    n++;
                if (neigh[n] = get(x,   y-1))
                    n++;
                if (neigh[n] = get(x+1, y))
                    n++;
                if (neigh[n] = get(x,   y+1))
                    n++;
                if (get(x,y)) {
                    if  (((n!=3) && (n!=2)))
                        set(x,y,0);
                    continue;
                }
                if (n == 3) {
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
        int *tmp = field;
        field = dbuf;
        dbuf = tmp;
    }
};

struct gofui_t {
    int vp_x, vp_y, w, h;

    gofui_t(int _w, int _h) {
        vp_x = vp_y = 0;
        w = _w; h = _h;
    }

    void resize(int nw, int nh) {
        w = nw; h = nh;
    }

    // something to process user input here.
};

gofsim_t *sim;
gofui_t *ui;

void mainloop_cb(void) {
    // makes things to render
    sim->tick();
}

void render_things_cb(void) {
    static const char colormap[] = { 0 };
    // renders things
    irenderer *r = getrenderer();
    df_buffer_t *buf = r->get_buffer();
    /* now render a portion of the map into the buffer
       according to its dimx/dimy
       this example assumes map cell values are indices
       into the texture list. this is not so in DF,
       and this mapping step, if necessary, is to be done here.

       the viewport position is to be kept where?? in the interface
       object, of course.
    */

    memset(buf->texpos, 0, buf->dimx * buf->dimy * 4);
    memset(buf->addcolor, 0, buf->dimx * buf->dimy);
    memset(buf->grayscale, 0, buf->dimx * buf->dimy);
    memset(buf->cf, 0, buf->dimx * buf->dimy);
    memset(buf->cbr, 0, buf->dimx * buf->dimy);

    for (int i = 0; i < ui->w; i++)
        for (int j = 0; j < ui->h; j++) {
            int index = ( buf->dimy * i + j );
            int value = sim->get( ( i + ui->vp_x ) % ui->w,
                                   ( j + ui->vp_y ) % ui->h );
            buf->screen[4*index] = 111; // 'o'
            buf->screen[4*index+1] = value; // fg, 1:1 map to ansi is nice
            buf->screen[4*index+2] = 0; // bg
            buf->screen[4*index+3] = 0; // br
            buf->texpos[index] = value;
        }
    r->release_buffer(buf);
}

void gps_resize_cb(int w, int h) {
    // grid resize notification sink
    ui->resize(w, h);
}

void render_thread() {
    iplatform *p = getplatform();
    itextures *tm = gettextures();
    ikeyboard *k  = getkeyboard();
    imusicsound *ms = getmusicsound();
}

int main (int argc, char* argv[]) {
    if (argc != 4) {
	fprintf(stderr, "Usage: ./test platform-name sound-name renderer-name\n");
	return 1;
    }
    load_modules(argv[1], argv[2], argv[3]);

    sim = new gofsim_t(800, 250);
    ui = new gofui_t(80, 25);

    // get platform - trigger initialization
    iplatform *platform = getplatform();
    iplatform->log_info("starting. or maybe staring.");
    irenderer *renderer = getrenderer();
    isimuloop *simuloop = getsimuloop();

    // set up the simuloop
    simuloop->set_callbacks(render_things_cb, mainloop_cb, gps_resize_cb);
    // run the rendering loop
    renderer->loop();

    delete sim;
    return 0;
}
