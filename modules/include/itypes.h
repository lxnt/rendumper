#if !defined(ITYPES_H)
#define ITYPES_H

#include <cstdint>

#if defined(__WIN32__) || defined(__CYGWIN__)
# define DFM_EXPORT __declspec(dllexport)
# define DFM_APIEP __cdecl
#else /* GCC 4+ is implied. -fvisibility=hidden is implied. */
# define DFM_EXPORT __attribute__((visibility ("default")))
# define DFM_APIEP
#endif

/* forward decls of everything for the deps */
struct iplatform;
struct imqueue;
struct itextures;
struct irenderer;
struct isimuloop;
struct imusicsound;
struct ikeyboard;

/* entry point pointer typedefs for the deps */
extern "C" {
typedef iplatform * (DFM_APIEP *getplatform_t)(void);
typedef imqueue   * (DFM_APIEP *getmqueue_t)(void);
typedef irenderer * (DFM_APIEP *getrenderer_t)(void);
typedef itextures * (DFM_APIEP *gettextures_t)(void);
typedef isimuloop * (DFM_APIEP *getsimuloop_t)(void);
typedef ikeyboard * (DFM_APIEP *getkeyboard_t)(void);
typedef imusicsound * (DFM_APIEP *getmusicsound_t)(void);
typedef void (*dep_foo_t)(getplatform_t **, getmqueue_t **,
                          getrenderer_t **, gettextures_t **,
                          getsimuloop_t **, getmusicsound_t **,
                          getkeyboard_t **);

}



/*  A buffer for passing around screen data.
    screen is guaranteed to be page-aligned,
    rest of pointers  - 64bit-aligned.

    tail points to grid_w*grid_h*tail_sizeof bytes,
    for lumping the whole shebang into a single bo
    or something. Not for use outside the backend.
*/
struct df_buffer_t {
    uint32_t w, h;
    uint32_t tail_sizeof;
    uint32_t required_sz, used_sz;
    uint8_t *ptr;
    uint32_t pstate;           // renderer-private state

    unsigned char *screen;     // uchar[4] in fact.
    long *texpos;
    char *addcolor;
    unsigned char *grayscale;
    unsigned char *cf;
    unsigned char *cbr;

    uint8_t *tail;
};

struct itc_message_t;

struct imessagesender {
    virtual void acknowledge(const itc_message_t&) = 0;
};

enum DFKeySym : int32_t {
/* Values accidentally coincide with SDL2's ones. We can map SDL12's ones
   to SDL2's with a simple int32_t[], but in the other direction we would
   need a hack. */

    DFKS_STOP = 0x40000078,
    DFKS_GREATER = 0x0000003e,
    DFKS_SEPARATOR = 0x4000009f,
    DFKS_KP_MULTIPLY = 0x40000055,
    DFKS_KBDILLUMUP = 0x40000118,
    DFKS_y = 0x00000079,
    DFKS_CARET = 0x0000005e,
    DFKS_CANCEL = 0x4000009b,
    DFKS_WWW = 0x40000108,
    DFKS_COMPUTER = 0x4000010b,
    DFKS_KP_MEMSUBTRACT = 0x400000d4,
    DFKS_KP_COLON = 0x400000cb,
    DFKS_KP_EQUALS = 0x40000067,
    DFKS_KP_MEMRECALL = 0x400000d1,
    DFKS_AC_BOOKMARKS = 0x40000112,
    DFKS_KP_HASH = 0x400000cc,
    DFKS_DELETE = 0x0000007f,
    DFKS_KP_AMPERSAND = 0x400000c7,
    DFKS_RETURN2 = 0x4000009e,
    DFKS_EXSEL = 0x400000a4,
    DFKS_QUOTEDBL = 0x00000022,
    DFKS_CUT = 0x4000007b,
    DFKS_KP_000 = 0x400000b1,
    DFKS_ALTERASE = 0x40000099,
    DFKS_CLEARAGAIN = 0x400000a2,
    DFKS_MODE = 0x40000101,
    DFKS_SLEEP = 0x4000011a,
    DFKS_AC_STOP = 0x40000110,
    DFKS_KP_BACKSPACE = 0x400000bb,
    DFKS_CRSEL = 0x400000a3,
    DFKS_CLEAR = 0x4000009c,
    DFKS_KP_RIGHTPAREN = 0x400000b7,
    DFKS_RCTRL = 0x400000e4,
    DFKS_MAIL = 0x40000109,
    DFKS_PASTE = 0x4000007d,
    DFKS_AUDIOSTOP = 0x40000104,
    DFKS_CAPSLOCK = 0x40000039,
    DFKS_COPY = 0x4000007c,
    DFKS_AMPERSAND = 0x00000026,
    DFKS_NUMLOCKCLEAR = 0x40000053,
    DFKS_DECIMALSEPARATOR = 0x400000b3,
    DFKS_KP_SPACE = 0x400000cd,
    DFKS_PERIOD = 0x0000002e,
    DFKS_DISPLAYSWITCH = 0x40000115,
    DFKS_AUDIONEXT = 0x40000102,
    DFKS_CURRENCYSUBUNIT = 0x400000b5,
    DFKS_KP_BINARY = 0x400000da,
    DFKS_CURRENCYUNIT = 0x400000b4,
    DFKS_QUOTE = 0x00000027,
    DFKS_KP_POWER = 0x400000c3,
    DFKS_KBDILLUMDOWN = 0x40000117,
    DFKS_HASH = 0x00000023,
    DFKS_LSHIFT = 0x400000e1,
    DFKS_j = 0x0000006a,
    DFKS_OPER = 0x400000a1,
    DFKS_VOLUMEUP = 0x40000080,
    DFKS_AC_REFRESH = 0x40000111,
    DFKS_KP_MEMMULTIPLY = 0x400000d5,
    DFKS_KP_GREATER = 0x400000c6,
    DFKS_RGUI = 0x400000e7,
    DFKS_MEDIASELECT = 0x40000107,
    DFKS_AC_FORWARD = 0x4000010f,
    DFKS_KP_XOR = 0x400000c2,
    DFKS_SEMICOLON = 0x0000003b,
    DFKS_PLUS = 0x0000002b,
    DFKS_KP_OCTAL = 0x400000db,
    DFKS_F24 = 0x40000073,
    DFKS_F23 = 0x40000072,
    DFKS_F22 = 0x40000071,
    DFKS_F21 = 0x40000070,
    DFKS_OUT = 0x400000a0,
    DFKS_SYSREQ = 0x4000009a,
    DFKS_KP_EXCLAM = 0x400000cf,
    DFKS_EXECUTE = 0x40000074,
    DFKS_KP_MEMADD = 0x400000d3,
    DFKS_KP_8 = 0x40000060,
    DFKS_KP_9 = 0x40000061,
    DFKS_UNDO = 0x4000007a,
    DFKS_KP_E = 0x400000c0,
    DFKS_KP_4 = 0x4000005c,
    DFKS_KP_5 = 0x4000005d,
    DFKS_KP_6 = 0x4000005e,
    DFKS_KP_7 = 0x4000005f,
    DFKS_KP_0 = 0x40000062,
    DFKS_KP_RIGHTBRACE = 0x400000b9,
    DFKS_KP_2 = 0x4000005a,
    DFKS_KP_3 = 0x4000005b,
    DFKS_AUDIOPREV = 0x40000103,
    DFKS_KP_F = 0x400000c1,
    DFKS_RIGHTBRACKET = 0x0000005d,
    DFKS_KP_D = 0x400000bf,
    DFKS_HELP = 0x40000075,
    DFKS_PRINTSCREEN = 0x40000046,
    DFKS_KP_PERIOD = 0x40000063,
    DFKS_KP_B = 0x400000bd,
    DFKS_KP_A = 0x400000bc,
    DFKS_F12 = 0x40000045,
    DFKS_EQUALS = 0x0000003d,
    DFKS_F10 = 0x40000043,
    DFKS_KP_EQUALSAS400 = 0x40000086,
    DFKS_F16 = 0x4000006b,
    DFKS_F17 = 0x4000006c,
    DFKS_F14 = 0x40000069,
    DFKS_F15 = 0x4000006a,
    DFKS_BACKSLASH = 0x0000005c,
    DFKS_F18 = 0x4000006d,
    DFKS_KP_C = 0x400000be,
    DFKS_UNKNOWN = 0x00000000,
    DFKS_AUDIOPLAY = 0x40000105,
    DFKS_UNDERSCORE = 0x0000005f,
    DFKS_AC_HOME = 0x4000010d,
    DFKS_KP_MEMDIVIDE = 0x400000d6,
    DFKS_SELECT = 0x40000077,
    DFKS_MUTE = 0x4000007f,
    DFKS_LCTRL = 0x400000e0,
    DFKS_F11 = 0x40000044,
    DFKS_RALT = 0x400000e6,
    DFKS_KP_PLUSMINUS = 0x400000d7,
    DFKS_CALCULATOR = 0x4000010a,
    DFKS_LESS = 0x0000003c,
    DFKS_KP_LEFTPAREN = 0x400000b6,
    DFKS_INSERT = 0x40000049,
    DFKS_RETURN = 0x0000000d,
    DFKS_KP_PERCENT = 0x400000c4,
    DFKS_KP_AT = 0x400000ce,
    DFKS_PAUSE = 0x40000048,
    DFKS_SCROLLLOCK = 0x40000047,
    DFKS_KP_TAB = 0x400000ba,
    DFKS_KP_ENTER = 0x40000058,
    DFKS_F19 = 0x4000006e,
    DFKS_ESCAPE = 0x0000001b,
    DFKS_ASTERISK = 0x0000002a,
    DFKS_F13 = 0x40000068,
    DFKS_KP_DBLVERTICALBAR = 0x400000ca,
    DFKS_AUDIOMUTE = 0x40000106,
    DFKS_RSHIFT = 0x400000e5,
    DFKS_FIND = 0x4000007e,
    DFKS_KP_MINUS = 0x40000056,
    DFKS_KP_DECIMAL = 0x400000dc,
    DFKS_F20 = 0x4000006f,
    DFKS_KP_1 = 0x40000059,
    DFKS_KP_LESS = 0x400000c5,
    DFKS_MINUS = 0x0000002d,
    DFKS_PAGEUP = 0x4000004b,
    DFKS_COLON = 0x0000003a,
    DFKS_BRIGHTNESSUP = 0x40000114,
    DFKS_COMMA = 0x0000002c,
    DFKS_BRIGHTNESSDOWN = 0x40000113,
    DFKS_QUESTION = 0x0000003f,
    DFKS_EJECT = 0x40000119,
    DFKS_KP_PLUS = 0x40000057,
    DFKS_AT = 0x00000040,
    DFKS_LGUI = 0x400000e3,
    DFKS_END = 0x4000004d,
    DFKS_KP_CLEARENTRY = 0x400000d9,
    DFKS_TAB = 0x00000009,
    DFKS_DOLLAR = 0x00000024,
    DFKS_MENU = 0x40000076,
    DFKS_KBDILLUMTOGGLE = 0x40000116,
    DFKS_POWER = 0x40000066,
    DFKS_PAGEDOWN = 0x4000004e,
    DFKS_HOME = 0x4000004a,
    DFKS_9 = 0x00000039,
    DFKS_VOLUMEDOWN = 0x40000081,
    DFKS_APPLICATION = 0x40000065,
    DFKS_LEFT = 0x40000050,
    DFKS_KP_MEMSTORE = 0x400000d0,
    DFKS_RIGHT = 0x4000004f,
    DFKS_LEFTPAREN = 0x00000028,
    DFKS_DOWN = 0x40000051,
    DFKS_UP = 0x40000052,
    DFKS_u = 0x00000075,
    DFKS_t = 0x00000074,
    DFKS_w = 0x00000077,
    DFKS_KP_HEXADECIMAL = 0x400000dd,
    DFKS_q = 0x00000071,
    DFKS_KP_MEMCLEAR = 0x400000d2,
    DFKS_s = 0x00000073,
    DFKS_r = 0x00000072,
    DFKS_THOUSANDSSEPARATOR = 0x400000b2,
    DFKS_PERCENT = 0x00000025,
    DFKS_x = 0x00000078,
    DFKS_z = 0x0000007a,
    DFKS_e = 0x00000065,
    DFKS_d = 0x00000064,
    DFKS_g = 0x00000067,
    DFKS_f = 0x00000066,
    DFKS_a = 0x00000061,
    DFKS_c = 0x00000063,
    DFKS_b = 0x00000062,
    DFKS_m = 0x0000006d,
    DFKS_AGAIN = 0x40000079,
    DFKS_o = 0x0000006f,
    DFKS_n = 0x0000006e,
    DFKS_i = 0x00000069,
    DFKS_h = 0x00000068,
    DFKS_k = 0x0000006b,
    DFKS_AC_SEARCH = 0x4000010c,
    DFKS_F8 = 0x40000041,
    DFKS_F9 = 0x40000042,
    DFKS_KP_CLEAR = 0x400000d8,
    DFKS_l = 0x0000006c,
    DFKS_AC_BACK = 0x4000010e,
    DFKS_SLASH = 0x0000002f,
    DFKS_KP_LEFTBRACE = 0x400000b8,
    DFKS_F1 = 0x4000003a,
    DFKS_F2 = 0x4000003b,
    DFKS_F3 = 0x4000003c,
    DFKS_F4 = 0x4000003d,
    DFKS_F5 = 0x4000003e,
    DFKS_F6 = 0x4000003f,
    DFKS_F7 = 0x40000040,
    DFKS_KP_VERTICALBAR = 0x400000c9,
    DFKS_v = 0x00000076,
    DFKS_LALT = 0x400000e2,
    DFKS_BACKQUOTE = 0x00000060,
    DFKS_KP_00 = 0x400000b0,
    DFKS_EXCLAIM = 0x00000021,
    DFKS_p = 0x00000070,
    DFKS_5 = 0x00000035,
    DFKS_4 = 0x00000034,
    DFKS_7 = 0x00000037,
    DFKS_6 = 0x00000036,
    DFKS_1 = 0x00000031,
    DFKS_0 = 0x00000030,
    DFKS_3 = 0x00000033,
    DFKS_2 = 0x00000032,
    DFKS_RIGHTPAREN = 0x00000029,
    DFKS_8 = 0x00000038,
    DFKS_KP_DIVIDE = 0x40000054,
    DFKS_KP_DBLAMPERSAND = 0x400000c8,
    DFKS_BACKSPACE = 0x00000008,
    DFKS_LEFTBRACKET = 0x0000005b,
    DFKS_PRIOR = 0x4000009d,
    DFKS_KP_COMMA = 0x40000085,
    DFKS_SPACE = 0x00000020
};

#define DFMOD_NONE 0
#define DFMOD_SHIFT 1
#define DFMOD_CTRL 2
#define DFMOD_ALT 4

struct df_input_event_t {
    enum ev_type : uint8_t { DF_KEY_UP, DF_KEY_DOWN, DF_BUTTON_UP, DF_BUTTON_DOWN, DF_QUIT } type;
    enum but_num : uint8_t { DF_BUTTON_LEFT, DF_BUTTON_RIGHT, DF_BUTTON_MIDDLE, DF_WHEEL_UP, DF_WHEEL_DOWN } button;
    uint16_t mod;
    DFKeySym sym;
    int32_t button_grid_x, button_grid_y;
    int32_t unicode;
    bool reports_release;
    uint32_t now;
};

/* This is not a part of irenderer/isimuloop C++ interfaces,
   rather of their mqueue ones.

   Messages are sent by calling appropriate method on the recipient
   instance. Its implementation is responsible for the gory details,
   such as queue name, etc.

   Commands, generally, should be passed by setting corresponding
   bool flags in the recipient instance via its method. Remember,
   that anything less that 4 bytes aligned to 4 bytes is likely read
   and written atomically on our target platforms, and even if it's not,
   a single bool value can be either true or false, never in between.

   Only when passing a compound data type or when acknowledgement
   is required is the use of mqueues acceptable.

*/
struct itc_message_t {
    enum msg_t {
	quit, 		// async_msg - to main (renderer) thread
	complete,
	set_fps,
	set_gfps,
	push_resize,
	pop_resize,
	reset_textures,
	pause, 		// async_cmd - from main (renderer) thread
	start,          //      unpause
	render,         //      be obvious
	inc,            // 	run extra simulation frames
	zoom_in, 	// zoom_commands - to main (renderer) thread
	zoom_out,
	zoom_reset,
	zoom_fullscreen,
	zoom_resetgrid,

        render_buffer,  // buffer submission to the renderer
        input_event   // renderthread->simuthread, ncurses input in d.inp

    } t;
    imessagesender *sender;
    union {
        int fps;
        df_buffer_t *buffer;
        df_input_event_t event;
    } d;
};

#if defined(SDL_INIT_VIDEO)
struct df_taindex_entry_t {
    SDL_Rect rect;
    bool magentic;
    bool gray;
};
struct df_texalbum_t {
    SDL_Surface *album;
    df_taindex_entry_t *index;
    uint32_t count;
    uint32_t height;
};
#else
struct df_texalbum_t;
#endif

#endif
