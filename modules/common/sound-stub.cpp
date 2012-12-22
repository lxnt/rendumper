/* a stub music-sound module, intended as an example and/or as an implementation for [SOUND:NO] */
#include "common_code_deps.h"

#define DFMODULE_BUILD
#include "imusicsound.h"

namespace {

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
    platform->log_info("stub_sound: update()\n");
}
void implementation::set_master_volume(long newvol)  {
    platform->log_info("stub_sound: set_master_volume(%ld)\n", newvol);
}
void implementation::load_sound(const char *filename, int slot, bool is_song) {
    platform->log_info("stub_sound: update(filename='%s', slot=%d, is_song=%s)\n",
        filename, slot, is_song?"true":"false");
}
void implementation::play_sound(int slot, bool is_song) {
    platform->log_info("stub_sound: play_sound(slot=%d, is_song=%s)\n", slot, is_song?"true":"false");
}
void implementation::start_background(int slot, bool is_song) {
    platform->log_info("stub_sound: start_background(slot=%d, is_song=%s)\n", slot, is_song?"true":"false");
}
void implementation::stop_background(void) {
    platform->log_info("stub_sound: stop_background()\n");
}
void implementation::stop_all(void) {
    platform->log_info("stub_sound: stop_all()\n");
}
void implementation::stop_sound(int slot, bool is_song) {
    platform->log_info("stub_sound: stop_sound(slot=%d, is_song=%s)\n", slot, is_song?"true":"false");
}
void implementation::release(void) {
    platform->log_info("stub_sound: release()\n");
}
static implementation the_stub;

}

extern "C" DECLSPEC imusicsound * APIENTRY getmusicsound(void) {
    return &the_stub;
}
