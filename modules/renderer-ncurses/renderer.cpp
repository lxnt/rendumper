#include "ncurses.h"

#define DFMODULE_BUILD
#include "irenderer.h"

struct r_implementation : public irenderer {
    void release(void);

    void zoom_in(void);
    void zoom_out(void);
    void zoom_reset(void);
    void toggle_fullscreen(void;
    void override_grid_size(void);
    void release_grid_size(void); 
    int mouse_state(int *mx, int *my); // ala sdl2's one.
    
    df_buffer_t *get_buffer(void);
    void release_buffer(df_buffer_t *buf);
    
private:
    WINDOW *win;
    
};


static r_implementation *impl = NULL;

extern "C" DECLSPEC irenderer * APIENTRY getrenderer(void) {
    if (!impl)
        impl = new r_implementation();
    return impl;
}

r_implementation::r_implementation() {
    win = initscr();
    cbreak();
    keypad(stdscr, TRUE);
    noecho();
    curs_set(0);
    start_color();      
    init_pair(1, COLOR_WHITE, COLOR_BLACK);

    fprintf(stderr, "initialized.\n");
}

void r_implementation::release(void) { 
    fprintf(stderr, "released.\n");
}