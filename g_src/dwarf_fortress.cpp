#include <string>
#include <stdint.h>

#include "random.h"
#include "init.h"
#include "graphics.h"
#include "enabler.h"
#include "interface.h"
#include "texture_handler.h"
#include "music_and_sound_g.h"

initst init;
texture_handlerst texture;
GameMode gamemode;
GameType gametype;
uint32_t movie_version = 23;
interfacest gview;
short mt_virtual_buffer = 8;
int32_t basic_seed = 0xB00B5;
short mt_cur_buffer = 23;
uint32_t mt_buffer[MT_BUFFER_NUM][MT_LEN];
int mt_index[MT_BUFFER_NUM];
std::string errorlog_prefix("errorlog_prefix");

void viewscreenst::help() { }
bool viewscreenst::key_conflict(long) { return false; }
void drawborder(char const*, char, char const*) { }
void texture_handlerst::clean() { }
void process_object_lines(textlinesst &, std::string &, std::string &) { }
void dwarf_end_announcements() { }
void dwarf_remove_screen() { }
void dwarf_option_screen() { }
char standardscrolling(std::set<InterfaceKey> &, short &, int32_t, int32_t, int32_t, uint32_t) { return 23; }
char standardscrolling(std::set<InterfaceKey> &, int32_t &, int32_t, int32_t, int32_t, uint32_t) { return 42; }
void enablerst::reset_textures() { }

char beginroutine() { return 0; }
char mainloop() { return 0; }
void endroutine() { }
