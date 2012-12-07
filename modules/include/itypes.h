#if !defined(ITYPES_H)
#define ITYPES_H

#include <cstdint>


struct df_buffer_t {
    int dimx, dimy;

    unsigned char *screen;     // uchar[4] in fact.
    long *texpos;
    char *addcolor;
    unsigned char *grayscale;
    unsigned char *cf;
    unsigned char *cbr;
};

typedef char (*mainloop_foo_t)();
typedef void (*render_things_foo_t)();
typedef void (*assimilate_buffer_foo_t)(df_buffer_t *);
typedef void (*add_input_ncurses_foo_t)(int32_t, uint32_t);

/* this is not a part of irenderer/isimuloop C++ interfaces,
   rather of their mqueue ones. */
struct itc_message_t {
    enum msg_t {
	quit, 		// async_msg - to main (renderer) thread
	complete,
	set_fps,
	set_gfps,
	push_resize,
	pop_resize,
	reset_textures,
	pause, 		// async_cmd - from main (renderer) thread
	start,          //      unpause
	render,         //      be obvious
	inc,            // 	run extra simulation frames
	zoom_in, 	// zoom_commands - to main (renderer) thread
	zoom_out,
	zoom_reset,
	zoom_fullscreen,
	zoom_resetgrid,

        input_ncurses   // renderthread->simuthread, ncurses input in d.inp
    } t;
    union {
        int fps;
        struct {
             int32_t key;
            uint32_t now;
        } inp;
    } d;
};

#endif
