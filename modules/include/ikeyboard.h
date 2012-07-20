/*
    input events and keycodes. hmm. we should somehow refrain from
    depending on SDL constants. On the other hand, that'd mean 
    either duplicating them, or moving keymap transform into 
    the renderer. hmm again.

    Currently input events get fed to the enabler_inputst::add_input
    as they come. 

    On the other side, they come out as set<InterfaceKey> out of 
    enabler_inputst::add_input(). InterfaceKey is typedef long, just 
    an enum. enabler_inputst does the mapping and macro expansion.
    
    at the very least, we need a method to return a sequence of input events,
    so that they can be fed into enabler_inputst::add_input_refined() when 
    the simulation thread is in mood of processing the input.
    
    hmm. There is that "static multimap<EventMatch,InterfaceKey> keymap;".
    struct EventMatch holds SDLKey key. 
    an input event is (complicately) translated into an EventMatch 
    
    1. enabler_inputst::add_input() gets an SDL key event.
        a bunch of KeyEvents maybe pushed into   list<pair<KeyEvent, int serial> > synthetics;
        event itself is transformed into a KeyEvent.
        the synthetics list gets forwarded into enabler_inputst::add_input_refined()
    2. enabler_inputst::add_input_refined()
       bah. it's complicated.
       
    Anyway, we replace sdl and keysyms with our own. Note how numeric values are not defined
    anywhere. This is possible because the game maps keys either by unicode value, 
    or by the sym names. This gives us much needed possibility to not depend on any one 
    keycode sequence, be it SDL 1.2, SDL 2.0 or whatever else.
    
    All we need is to recognize the names used in the interface.txt and try to sanely map them
    to whatever backend we happen to use.
    
    Thus the names are a part of this interface. They were in the sdlNames map:
    
    Backspace Tab Clear Enter Pause ESC Space Exclaim Quotedbl Hash Dollar Ampersand 
    Quote Leftparen Rightparen Asterisk Plus Comma Minus Period Slash 0 1 2 3 4 5 6 7 8 9 
    Colon Semicolon Less Equals Greater Question At Leftbracket Backslash Rightbracket 
    Caret Underscore Backquote a b c d e f g h i j k l m n o p q r s t u v w x y z Delete 
    Numpad 0 Numpad 1 Numpad 2 Numpad 3 Numpad 4 Numpad 5 Numpad 6 Numpad 7 Numpad 8 Numpad 9 
    Numpad Period Numpad Divide Numpad Multiply Numpad Plus Numpad Minus Numpad Enter 
    Numpad Equals Up Down Right Left Insert Home End Page Up Page Down 
    F1 F2 F3 F4 F5 F6 F7 F8 F9 F10 F11 F12 F13 F14 F15 Numlock Capslock Scrollock 
    Rshift Lshift Rctrl Lctrl Ralt Lalt Rmeta Lmeta Lsuper Rsuper Mode Compose Help 
    Print Sysreq Break Menu Power Euro Undo

    
*/

#include "ideclspec.h"

#if defined (DFMODULE_BUILD)
# define DFMOD_SHIFT 1 // from enabler_input.h
# define DFMOD_CTRL 2
# define DFMOD_ALT 4
#endif

struct event_t {
    bool released;
    enum event_type_t { iev_unicode, iev_key, iev_button, iev_quit } type;
    unsigned short unicode;  // unicode code point
    unsigned short key;      // opaque keycode.
    unsigned short mod;      // modifier state, DFMOD_ bits.
    unsigned short mousebutton; // 0=left 1=right? right?

};

struct ikeyboard {
    virtual void release() = 0;
    
    /* returns pending keyboard (and mousebutton) input in the 
	'buf' array of maximum 'buflen' elements in the order of arrival.
	return value is the number of elements written. if it is equal to buflen,
        there might be more. if it's zero, there are none.
    */
    virtual int get_input(const event_t *buf, int buflen) = 0;
    /* maps codes to names and back, so that interface.txt can be parsed and written back too. */
    virtual const char *key_name(const event_t e) = 0;
    virtual unsigned short key_name(const char *name) = 0;
};

#if defined (DFMODULE_BUILD) || defined(DFMODULE_IMPLICIT_LINK)
extern "C" DECLSPEC ikeyboard * APIENTRY getkeyboard(void);
#else // using glue and runtime loading.
extern "C" DECLSPEC ikeyboard * APIENTRY (*getkeyboard)(void);
#endif
