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


struct itc_message_t;

struct imessagesender {
    virtual void acknowledge(const itc_message_t&) = 0;
};

/* This is not a part of irenderer/isimuloop C++ interfaces,
   rather of their mqueue ones.

   Messages are sent by calling appropriate method on the recipient
   instance. Its implementation is responsible for the gory details,
   such as queue name, etc.

   Commands, generally, should be passed by setting corresponding
   bool flags in the recipient instance via its method. Remember,
   that anything less that 4 bytes aligned to 4 bytes is likely read
   and written atomically on our target platforms, and even if it's not,
   a single bool value can be either true or false, never in between.

   Only when passing a compound data type or when acknowledgement
   is required is the use of mqueues acceptable.

*/
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

        render_buffer,  // buffer submission to the renderer
        input_ncurses   // renderthread->simuthread, ncurses input in d.inp

    } t;
    imessagesender *sender;
    union {
        int fps;
        df_buffer_t *buffer;
        struct {
             int32_t key;
            uint32_t now;
        } inp;
    } d;
};

#endif
