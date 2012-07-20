/* glue code for "explicit link" in windows style - that is,
   using dlopen/
   
    client code should be compiled with DFMODULE_IMPLICIT_LINK 
    undefined, and then linked with this object file.
*/

#if defined(WIN32)
# include <windows.h>
# define _load_lib(name) LoadLibrary(name)
# define _get_sym(lib, name) GetProcAddress((HINSTANCE)lib, name);
# define _error "unknown error"
# define _release_lib(lib) FreeLibrary((HINSTANCE)lib)
# define _prefix "libs/"
# define _suffix ".dll"
#else
# include <dlfcn.h>
# define _load_lib(name) dlopen(name, RTLD_NOW)
# define _error dlerror()
# define _get_sym(lib, name) dlsym(lib, name);
# define _release_lib(name) dlclose(lib)
# define _prefix "libs/lib"
# define _suffix ".so"
#endif

#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <string>

#include "iplatform.h"
#include "irenderer.h"
#include "ikeyboard.h"
#include "itextures.h"
#include "imusicsound.h"


static void *plat_lib, *sound_lib, *render_lib;

typedef itextures * (*gettextures_t)(void);
typedef isimuloop * (*getsimuloop_t)(void);
typedef irenderer * (*getrenderer_t)(void);
typedef ikeyboard * (*getkeyboard_t)(void);
typedef imusicsound * (*getmusicsound_t)(void);
typedef iplatform * (*getplatform_t)(void);

gettextures_t gettextures;
getsimuloop_t getsimuloop;
getrenderer_t getrenderer;
getkeyboard_t getkeyboard;
getmusicsound_t getmusicsound;
getplatform_t getplatform;

void load_modules(const char * platform, const char * sound, const char * renderer) {
    std::string fname(_prefix);
    fname += "plat_";
    fname += platform;
    fname += _suffix;
    
    plat_lib = _load_lib(fname.c_str());
    if (!plat_lib) {
        fprintf(stderr, "_load_lib(%s): %s\n", fname.c_str(), _error);
        exit(1);
    }
    getplatform = (getplatform_t)_get_sym(plat_lib, "getplatform");
    if (!getplatform) {
        fprintf(stderr, "_get_sym(%s): %s\n", "getplatform", _error);
        exit(1);
    }
    
    fname = _prefix;
    fname += "sound_";
    fname += sound;
    fname += _suffix;
    
    sound_lib = _load_lib(fname.c_str());
    if (!plat_lib) {
        fprintf(stderr, "_load_lib(%s): %s\n", fname.c_str(), _error);
        exit(1);
    }
    getmusicsound = (getmusicsound_t)_get_sym(sound_lib, "getmusicsound");
    if (!getmusicsound) {
        fprintf(stderr, "_get_sym(%s) failed.\n", "getmusicsound");
        exit(1);
    }
    
    fname = _prefix;
    fname += "render_";
    fname += renderer;
    fname += _suffix;
    
    render_lib = _load_lib(fname.c_str());
    if (!render_lib) {
        fprintf(stderr, "_load_lib(%s): %s\n", fname.c_str(), _error);
        exit(1);
    }
    gettextures = (gettextures_t)_get_sym(render_lib, "gettextures");
    if (!gettextures) {
        fprintf(stderr, "_get_sym(%s) failed.\n", "gettextures");
        exit(1);
    }
    getsimuloop = (getsimuloop_t)_get_sym(render_lib, "getsimuloop");
    if (!getsimuloop) {
        fprintf(stderr, "_get_sym(%s) failed.\n", "getsimuloop");
        exit(1);
    }
    getrenderer = (getrenderer_t)_get_sym(render_lib, "getrenderer");
    if (!getrenderer) {
        fprintf(stderr, "_get_sym(%s) failed.\n", "getrenderer");
        exit(1);
    }
    getkeyboard = (getkeyboard_t)_get_sym(render_lib, "getkeyboard");
    if (!getkeyboard) {
        fprintf(stderr, "_get_sym(%s) failed.\n", "getkeyboard");
        exit(1);
    }
}
