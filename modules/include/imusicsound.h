#if !defined(IMUSICSOUND_H)
#define IMUSICSOUND_H

/* musicsound interface 
    typedef std::pair<bool,int> slot; has been converted to int slot, bool is_song.
    std::string &filename has been converted to const char *.
*/

#include "ideclspec.h"

struct imusicsound {
    virtual void release() = 0;
    
    virtual void update() = 0;
    virtual void set_master_volume(long newvol)  = 0;
    virtual void load_sound(const char *filename_cc, int slot, bool is_song) = 0;
    virtual void play_sound(int slot, bool is_song) = 0;
    virtual void start_background(int slot, bool is_song) = 0;
    virtual void stop_background(void) = 0;
    virtual void stop_all(void) = 0;
    virtual void stop_sound(int slot, bool is_song)  = 0;    
    
};
#if defined (DFMODULE_BUILD) || defined(DFMODULE_IMPLICIT_LINK)
extern "C" DECLSPEC imusicsound * APIENTRY getmusicsound(void);
#else // using glue and runtime loading.
extern "C" DECLSPEC imusicsound * APIENTRY (*getmusicsound)(void);
#endif

#endif
