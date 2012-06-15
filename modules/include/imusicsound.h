/* for the time being, use SDL's begin_code.h for the DECLSPEC (DFAPI here)
   which defines necessary dllimport/export stuff */
#if defined(DFAPI_EXPORT) // building the dlls
# define BUILD_SDL
#endif
#include "begin_code.h"
#include "end_code.h" // struct packing is not used here.
#define DFAPI DECLSPEC
#define APIENTRY __stdcall

/* musicsound interface 

// If the bool is false, a sound; otherwise a song
typedef std::pair<bool,int> slot;

has been converted to int second, bool first.
std::string &filename has been converted to const char *.


*/
struct imusicsound {
    virtual void release() = 0;
    
    virtual void update() = 0;
    virtual void set_master_volume(long newvol)  = 0;
    virtual void set_song(const char *filename, int slot, bool is_song = false) = 0;
    virtual void set_sound(const char *filename, int slot, int pan=-1, int priority=0) = 0;
    virtual void playsound(bool first, int slot, bool is_song = false) = 0;
    virtual void playsound(int s,int channel) = 0;
    virtual void startbackgroundmusic(int slot, bool is_song = true) = 0;
    virtual void stopbackgroundmusic(void) = 0;
    virtual void stop_sound(void) = 0;
    virtual void stop_sound(int slot, bool is_song = false) = 0;
};
extern "C" DFAPI imusicsound * APIENTRY getmusicsound(void);
