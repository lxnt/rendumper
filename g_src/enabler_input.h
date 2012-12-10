#ifndef ENABLER_INPUT_H
#define ENABLER_INPUT_H

#include <string>
#include <set>
#include <list>

#include "ViewBase.h"
#include "keybindings.h"

#include "itypes.h"

typedef uint32_t Time;

enum Repeat {
  REPEAT_NOT,  // Don't repeat at all. Furthermore, cancel other repeats.
  REPEAT_SLOW, // Repeat normally.
  REPEAT_FAST  // Repeat instantly, without waiting for the first-repeat interval.
};

enum MatchType { type_unicode, type_key, type_button };

uint8_t getModState();
std::string translate_mod(uint8_t mod);
int decode_utf8(const std::string &s);
std::string encode_utf8(int unicode);

struct EventMatch {
  MatchType type;
  uint8_t mod;      // not defined for type=unicode. 1: shift, 2: ctrl, 4:alt
  uint8_t scancode; // not defined for type=button; unused in lieu of the sym
  union {
    uint16_t unicode;
    DFKeySym sym;
    uint8_t button;
  };
  
  bool operator== (const EventMatch &other) const {
    if (mod != other.mod) return false;
    if (type != other.type) return false;
    switch (type) {
    case type_unicode: return unicode == other.unicode;
    case type_key: return sym == other.sym;
    case type_button: return button == other.button;
    default: return false;
    }
  }
  
  bool operator< (const EventMatch &other) const {
    if (mod != other.mod) return mod < other.mod;
    if (type != other.type) return type < other.type;
    switch (type) {
    case type_unicode: return unicode < other.unicode;
    case type_key: return sym < other.sym;
    case type_button: return button < other.button;
    default: return false;
    }
  }
};

struct KeyEvent {
  bool release;
  EventMatch match;
};

typedef std::list<std::set<InterfaceKey> > macro;

struct RegisteredKey {
  MatchType type;
  string display;
};

class enabler_inputst {
  std::set<InterfaceKey> key_translation(EventMatch &match);
 public:
  Repeat key_repeat(InterfaceKey);
  void key_repeat(InterfaceKey, Repeat);
  void load_macro_from_file(const std::string &file);
  void save_macro_to_file(const std::string &file, const std::string &name, const macro &);
  
  // In practice.. do not use this one.
  void add_input(df_input_event_t& e);
  // Use this one. It's much nicer.
  void add_input_refined(KeyEvent &e, Time now, int serial);
  // Made specifically for curses. <0 = unicode, >0 = ncurses symbols.
  void add_input_ncurses(df_input_event_t& event);

  std::set<InterfaceKey> get_input(Time now);
  void clear_input();

  void load_keybindings(const std::string &file);
  void save_keybindings(const std::string &file);
  void save_keybindings();
  std::string GetKeyDisplay(int binding);
  std::string GetBindingDisplay(int binding);
  std::string GetBindingTextDisplay(int binding);

  // Macros
  void record_input(); // Records input until such a time as you say stop
  void record_stop(); // Stops recording, saving it as the active macro
  bool is_recording();
  void play_macro(); // Runs the active macro, if any
  bool is_macro_playing();
  std::list<string> list_macros();
  void load_macro(string name); // Loads some macro as the active one
  void save_macro(string name); // Saves the active macro under some name
  void delete_macro(string name);

  // Prefix commands
  bool prefix_building();
  void prefix_toggle();
  void prefix_add_digit(char digit);
  int prefix_end();
  string prefix();

  // Updating the key-bindings
  void register_key(); // Sets the next key-press to be stored instead of executed.
  list<RegisteredKey> getRegisteredKey(); // Returns a description of stored keys. Max one of each type.
  void bindRegisteredKey(MatchType type, InterfaceKey key); // Binds one of the stored keys to key
  bool is_registering(); // Returns true if we're still waiting for a key-hit

  std::list<EventMatch> list_keys(InterfaceKey key); // Returns a list of events matching this interfacekey
  void remove_key(InterfaceKey key, EventMatch ev); // Removes a particular matcher from the keymap.
};


#endif
