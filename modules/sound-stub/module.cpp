/* a stub music-sound module, intended as an example and/or as an implementation for [SOUND:NO] */
#define DFMODULE_BUILD
#include "imusicsound.h"


struct implementation : public imusicsound {
    virtual void release()  {};
    
    virtual void update() {};
    virtual void set_master_volume(long newvol)  {};
    virtual void load_sound(const char *filename, int slot, bool is_song) {};
    virtual void play_sound(int slot, bool is_song) {};
    virtual void start_background(int slot, bool is_song) {};
    virtual void stop_background(void) {};
    virtual void stop_all(void) {};
    virtual void stop_sound(int slot, bool is_song) {};
};

static implementation the_stub;

extern "C" DECLSPEC imusicsound * APIENTRY getmusicsound(void) { return &the_stub; }
