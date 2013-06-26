#include "SDL.h"
#include "SDL_mixer.h"

#include "iplatform.h"
#include "imqueue.h"
#include "itypes.h"

#define DFMODULE_BUILD
#include "imusicsound.h"

getplatform_t _getplatform = NULL;

namespace {

iplatform *platform = NULL;
ilogger *logr = NULL;
imqueue   *mqueue   = NULL;

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
    void gamelog_event(const char *text);
    
    implementation();
    int initialize();
    
    Mix_Music *music_slots[1024];
    int active_music_slot;
    int volume;
    int fadein_ms;
    int fadeout_ms;

};

int implementation::initialize() {
    int rate = MIX_DEFAULT_FREQUENCY;
    Uint16 format = MIX_DEFAULT_FORMAT;
    int channels = 2;
    int buffersize = 4096;
  
    memset(music_slots, 0, sizeof(music_slots));
    logr->info("sizeof(music_slots) = %d",sizeof(music_slots));
    
    if(SDL_Init(SDL_INIT_AUDIO) < 0) {
        logr->error("SDL_INIT_AUDIO: %s", SDL_GetError());
        return 1;
    }
    if (Mix_OpenAudio(rate, format, channels, buffersize) < 0) {
        logr->error( "Mix_OpenAudio(%d, %hx, %d, %d): %s",
            rate, format, channels, buffersize, SDL_GetError());
        return 1;
    } else {
        Mix_QuerySpec(&rate, &format, &channels);
        logr->info("Opened audio at %d Hz %d bit %s (%s), %d bytes audio buffer",rate,
            (format&0xFF),
            (channels > 2) ? "surround" : (channels > 1) ? "stereo" : "mono",
            (format&0x1000) ? "BE" : "LE", buffersize );
    }    

    volume = atoi(platform->get_setting("init.VOLUME", "255"));

    Mix_VolumeMusic(volume);

    return 0;
}

void implementation::update() {
    logr->trace("stub: update()");
}
void implementation::set_master_volume(long newvol)  {
    logr->trace("stub_sound: set_master_volume(%ld)", newvol);
}
void implementation::load_sound(const char *filename, int slot, bool is_song) {
    logr->trace("stub_sound: load_sound(filename='%s', slot=%d, is_song=%s)",
        filename, slot, is_song?"true":"false");
    if (is_song) {
        if (music_slots[slot] != NULL) {
            logr->warn("replacing music in slot %d with %s", slot, filename);
            if (active_music_slot == slot)
                logr->fatal("can't replace playing slot.");
            Mix_FreeMusic(music_slots[slot]);
        }
        music_slots[slot] = Mix_LoadMUS(filename);
        if (music_slots[slot] == NULL)
            logr->error("Mix_LoadMUS(%s): %s", filename, Mix_GetError());
    }
}
void implementation::play_sound(int slot, bool is_song) {
    logr->trace("stub_sound: play_sound(slot=%d, is_song=%s)", slot, is_song?"true":"false");
}
void implementation::start_background(int slot, bool is_song) {
    if (is_song) {
        if (active_music_slot == slot)
            return;
        if (music_slots[slot] == NULL) {
            logr->error("start_background(): slot %d not loaded", slot);
            return;
        }
        logr->info("playing music slot %d", slot);
        if (-1 == Mix_FadeInMusic(music_slots[slot], -1, fadein_ms))
            logr->error("Mix_FadeInMusic(%d, -1, 2000): %s", slot, Mix_GetError());
        active_music_slot = slot;
        return;
    }
    logr->error("stub_sound: start_background(slot=%d, is_song=%s)", slot, is_song?"true":"false");
}
void implementation::stop_background(void) {
    logr->info("stopping music (was playing slot %d)", active_music_slot);
    if (-1 == Mix_FadeOutMusic(fadeout_ms))
        logr->error("Mix_FadeOutMusic(%d): %s", fadeout_ms, Mix_GetError());
    active_music_slot = -1;
}
void implementation::stop_all(void) {
    logr->trace("stub_sound: stop_all()");
}
void implementation::stop_sound(int slot, bool is_song) {
    logr->trace("stub_sound: stop_sound(slot=%d, is_song=%s)", slot, is_song?"true":"false");
}
void implementation::gamelog_event(const char *text) {
    logr->trace("stub_sound: gamelog_event(%s)", text);
}
void implementation::release(void) {
    logr->trace("stub_sound: release()");
}

implementation::implementation() {
    active_music_slot = -1;
    volume = 255;
    fadein_ms = 1500;
    fadeout_ms = 1500;
}

/* module glue */

static implementation *impl = NULL;

getmqueue_t   _getmqueue = NULL;

extern "C" DFM_EXPORT void DFM_APIEP dependencies(
                            getplatform_t   **pl,
                            getmqueue_t     **mq,
                            getrenderer_t   **rr,
                            gettextures_t   **tx,
                            getsimuloop_t   **sl,
                            getmusicsound_t **ms,
                            getkeyboard_t   **kb) {
    *pl = &_getplatform;
    *mq = &_getmqueue;
    *rr = NULL;
    *tx = NULL;
    *sl = NULL;
    *ms = NULL;
    *kb = NULL;
}

extern "C" DFM_EXPORT imusicsound * DFM_APIEP getmusicsound(void) {
    if (!impl) {
        platform = _getplatform();
        mqueue   = _getmqueue();
        logr = platform->getlogr("sound");
        impl = new implementation();
        if (impl->initialize())
            impl = NULL;
    }
    return impl;
}
} /* ns */
