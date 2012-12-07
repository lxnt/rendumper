#include <string>
#include <utility>

#include "music_and_sound_g.h"

musicsoundst musicsound;

musicsoundst::~musicsoundst() { }
bool musicsoundst::initsound() {
    if (!getmusicsound) // no module loaded
        return false;
    if (!impl)
	impl = getmusicsound();
    if (!impl)  // it failed.
	return false;
    return true;
}
void musicsoundst::deinitsound() {
    if (impl) impl->release();
    impl = NULL;
}

void musicsoundst::update() {
    if (impl) impl->update();
}
void musicsoundst::set_master_volume(long newvol) {
    if (impl) impl->set_master_volume(newvol);
}
void musicsoundst::set_song(std::string &filename, slot slot) {
    if (impl) impl->load_sound(filename.c_str(), slot.second, slot.first);
}
void musicsoundst::set_song(std::string &filename, int slot) {
    if (impl) impl->load_sound(filename.c_str(), slot, true);
}
void musicsoundst::playsound(slot slot) {
    if (impl) impl->play_sound(slot.second, slot.first);
}
void musicsoundst::playsound(int slot) {
    if (impl) impl->play_sound(slot, false);
}
void musicsoundst::startbackgroundmusic(slot slot) {
    if (impl) impl->start_background(slot.second, slot.first);
}
void musicsoundst::startbackgroundmusic(int slot) {
    if (impl) impl->start_background(slot, true);
}
void musicsoundst::stopbackgroundmusic() {
    if (impl) impl->stop_background();
}
void musicsoundst::stop_sound() {
    if (impl) impl->stop_all();
}
void musicsoundst::stop_sound(slot slot) {
    if (impl) impl->stop_sound(slot.second, slot.first);
}
void musicsoundst::playsound(int s, int) {
    if (impl) impl->play_sound(s, false);
}
void musicsoundst::set_sound(std::string &filename, int slot, int, int) {
    if (impl) impl->load_sound(filename.c_str(), slot, false);
}

// Deprecated:
void musicsoundst::forcebackgroundmusic(int, unsigned long) { }
void musicsoundst::playsound(int s, int, int, int) {
    if (impl) impl->play_sound(s, false);
}
void musicsoundst::set_sound_params(int, int, int, int, int) { }
