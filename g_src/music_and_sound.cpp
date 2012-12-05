#include <string>
#include <utility>

#include "music_and_sound_g.h"
#include "imusicsound.h"

musicsoundst musicsound;

static imusicsound *impl = NULL;

musicsoundst::musicsoundst() { }
musicsoundst::~musicsoundst() { }
bool musicsoundst::initsound() { 
    if (!impl) 
	impl = getmusicsound();
    if (!impl)  // it failed.
	return false;
    return true;
}
void musicsoundst::deinitsound() { 
    impl->release(); 
    impl = NULL; 
}

void musicsoundst::update() { 
    impl->update(); 
}
void musicsoundst::set_master_volume(long newvol) { 
    impl->set_master_volume(newvol); 
}
void musicsoundst::set_song(std::string &filename, slot slot) {
    impl->load_sound(filename.c_str(), slot.second, slot.first);
}
void musicsoundst::set_song(std::string &filename, int slot) { 
    impl->load_sound(filename.c_str(), slot, true);
}
void musicsoundst::playsound(slot slot) {
    impl->play_sound(slot.second, slot.first);
}
void musicsoundst::playsound(int slot) {
    impl->play_sound(slot, false);
}
void musicsoundst::startbackgroundmusic(slot slot) {
    impl->start_background(slot.second, slot.first);
}
void musicsoundst::startbackgroundmusic(int slot) {
    impl->start_background(slot, true);
}
void musicsoundst::stopbackgroundmusic() { 
    impl->stop_background(); 
}
void musicsoundst::stop_sound() { 
    impl->stop_all(); 
}
void musicsoundst::stop_sound(slot slot) { 
    impl->stop_sound(slot.second, slot.first); 
}
void musicsoundst::playsound(int s,int channel) { 
    impl->play_sound(s, false); 
}
void musicsoundst::set_sound(std::string &filename, int slot, int pan, int priority) {
    impl->load_sound(filename.c_str(), slot, false);
}

// Deprecated:
void musicsoundst::forcebackgroundmusic(int slot, unsigned long time) { }
void musicsoundst::playsound(int s,int min_channel,int max_channel,int force_channel) {
    impl->play_sound(s, false);
}
void musicsoundst::set_sound_params(int slot,int p1,int vol,int pan,int priority) { }



