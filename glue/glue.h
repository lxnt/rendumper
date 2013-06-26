#if !defined(DF_MOD_GLUE_H)
#define DF_MOD_GLUE_H

#define DFMOD_EP_PLATFORM    1
#define DFMOD_EP_MQUEUE      2
#define DFMOD_EP_TEXTURES    4
#define DFMOD_EP_SIMULOOP    8
#define DFMOD_EP_RENDERER   16
#define DFMOD_EP_KEYBOARD   32
#define DFMOD_EP_MUSICSOUND 64

/* returns mask of interfaces provided by the shared object, or 0 on error */
int load_module(const char *soname);

/* attempts to load all modules the given renderer (printmode suffix) requires */
bool lock_and_load(const char *printmode, const char *modpath);

#endif
