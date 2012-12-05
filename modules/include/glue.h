#if !defined(DF_MOD_GLUE_H)
#define DF_MOD_GLUE_H

void set_modpath(const char *modpath);

#define DFMOD_EP_PLATFORM    1
#define DFMOD_EP_MQUEUE      2
#define DFMOD_EP_TEXTURES    4
#define DFMOD_EP_SIMULOOP    8
#define DFMOD_EP_RENDERER   16
#define DFMOD_EP_KEYBOARD   32
#define DFMOD_EP_MUSICSOUND 64

int load_module(const char *soname);

void load_modules(const char *modpath, const char *platform, const char *sound, const char *renderer);

#endif
