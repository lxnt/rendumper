/* a stub music-sound module, intended as an example and/or as an implementation for [SOUND:NO] */
#include "common_code_deps.h"

#define DFMODULE_BUILD
#include "imusicsound.h"

namespace {

ilogger *logr = NULL;

struct implementation : public imusicsound {
    void release();
    void update();
    void set_master_volume(long newvol);
    void load_sound(const char *filename, int slot, bool is_song);
    void play_sound(int slot, bool is_song);
    void start_background(int slot, bool is_song);
    void stop_background(void);
    void stop_all(void);
    void stop_sound(int slot, bool is_song);
};

void implementation::update() {
    logr->trace("stub_sound: update()\n");
}
void implementation::set_master_volume(long newvol)  {
    logr->trace("stub_sound: set_master_volume(%ld)\n", newvol);
}
void implementation::load_sound(const char *filename, int slot, bool is_song) {
    logr->trace("stub_sound: update(filename='%s', slot=%d, is_song=%s)\n",
        filename, slot, is_song?"true":"false");
}
void implementation::play_sound(int slot, bool is_song) {
    logr->trace("stub_sound: play_sound(slot=%d, is_song=%s)\n", slot, is_song?"true":"false");
}
void implementation::start_background(int slot, bool is_song) {
    logr->trace("stub_sound: start_background(slot=%d, is_song=%s)\n", slot, is_song?"true":"false");
}
void implementation::stop_background(void) {
    logr->trace("stub_sound: stop_background()\n");
}
void implementation::stop_all(void) {
    logr->trace("stub_sound: stop_all()\n");
}
void implementation::stop_sound(int slot, bool is_song) {
    logr->trace("stub_sound: stop_sound(slot=%d, is_song=%s)\n", slot, is_song?"true":"false");
}
void implementation::release(void) {
    logr->trace("stub_sound: release()\n");
}
static implementation the_stub;

}

extern "C" DFM_EXPORT imusicsound * DFM_APIEP getmusicsound(void) {
    if (!logr) {
        _get_deps();
        logr = platform->getlogr("cc.stub.sound");
    }
    return &the_stub;
}
