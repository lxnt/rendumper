/* glue code for "explicit link" in windows style - that is,
   using dlopen/

    client code should be compiled with DFMODULE_IMPLICIT_LINK
    undefined, and then linked with this object file.
*/

#include <cerrno>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>

#if defined(WIN32)
# include <windows.h>
# define _load_lib(name) LoadLibrary(name)
# define _get_sym(lib, name) GetProcAddress((HINSTANCE)lib, name)
# define _error "unknown error"
# define _release_lib(lib) FreeLibrary((HINSTANCE)lib)
const char * const _so_suffix = ".dll";
const char _os_path_sep = '\\';
#else
# include <dlfcn.h>
# define _load_lib(name) dlopen(name, RTLD_NOW)
# define _error dlerror()
# define _get_sym(lib, name) dlsym(lib, name)
# define _release_lib(name) dlclose(lib)
//const char * const _so_suffix = ".so";
const char _os_path_sep = '/';
const std::string _so_suffix(".so");
#endif

#undef DFMODULE_BUILD
#include "ideclspec.h"
#include "iplatform.h"
#include "imqueue.h"
#include "irenderer.h"
#include "isimuloop.h"
#include "ikeyboard.h"
#include "itextures.h"
#include "imusicsound.h"

typedef itextures * (*gettextures_t)(void);
typedef isimuloop * (*getsimuloop_t)(void);
typedef irenderer * (*getrenderer_t)(void);
typedef ikeyboard * (*getkeyboard_t)(void);
typedef imusicsound * (*getmusicsound_t)(void);
typedef iplatform * (*getplatform_t)(void);
typedef imqueue * (*getmqueue_t)(void);

getplatform_t getplatform = NULL;
getmqueue_t getmqueue = NULL;
gettextures_t gettextures = NULL;
getsimuloop_t getsimuloop = NULL;
getrenderer_t getrenderer = NULL;
getkeyboard_t getkeyboard = NULL;
getmusicsound_t getmusicsound = NULL;

static std::string module_path("");

/* blindly prepended to the soname */
void set_modpath(const char *modpath) {
    module_path = modpath;
    if    ((module_path.size() > 0)
        && (module_path.at(module_path.size() - 1) != _os_path_sep))
        module_path.append(1, _os_path_sep);
}

#define DFMOD_EP_PLATFORM    1
#define DFMOD_EP_MQUEUE      2
#define DFMOD_EP_TEXTURES    4
#define DFMOD_EP_SIMULOOP    8
#define DFMOD_EP_RENDERER   16
#define DFMOD_EP_KEYBOARD   32
#define DFMOD_EP_MUSICSOUND 64

/* returns a bitfield of which entry points were replaced.
   any error loading the so results in zero entry points replaced
   and thus return value of 0. */
int load_module(const char *soname) {
    std::string fname;

    fname = module_path;
    fname += soname;

    /* phew. fname += '' if fname.endswith(_so_suffix) else _so_suffix */
    if (! ((fname.size() >= _so_suffix.size())
        && (fname.compare(fname.size()-_so_suffix.size(), _so_suffix.size(), _so_suffix))== 0) )
        fname += _so_suffix;

    void *lib = _load_lib(fname.c_str());

    if (!lib) {
        if (!getplatform) {
            /* no platform yet. */
            fprintf(stderr, "load_module(\"%s\"): %s\n", fname.c_str(), _error);
        } else {
            getplatform()->log_error("load_module(%s): %s\n", fname.c_str(), _error);
        }
        return 0;
    }
    int rv = 0;
    void *sym;

    if ((sym = _get_sym(lib, "getplatform"))) {
        rv |= DFMOD_EP_PLATFORM; getplatform = (getplatform_t) sym;
    }

    if ((sym = _get_sym(lib, "getmqueue"))) {
        rv |= DFMOD_EP_MQUEUE; getmqueue = (getmqueue_t) sym;
    }

    if ((sym = _get_sym(lib, "gettextures"))) {
        rv |= DFMOD_EP_TEXTURES; gettextures = (gettextures_t) sym;
    }

    if ((sym = _get_sym(lib, "getsimuloop"))) {
        rv |= DFMOD_EP_SIMULOOP; getsimuloop = (getsimuloop_t) sym;
    }

    if ((sym = _get_sym(lib, "getrenderer"))) {
        rv |= DFMOD_EP_RENDERER; getrenderer = (getrenderer_t) sym;
    }

    if ((sym = _get_sym(lib, "getkeyboard"))) {
        rv |= DFMOD_EP_KEYBOARD; getkeyboard = (getkeyboard_t) sym;
    }

    if ((sym = _get_sym(lib, "getmusicsound"))) {
        rv |= DFMOD_EP_MUSICSOUND; getmusicsound = (getmusicsound_t) sym;
    }
    return rv;
}

void load_modules(const char *modpath, const char *platform, const char *sound, const char *renderer) {
    if (modpath)
        set_modpath(modpath);

    if (!platform) {
        fprintf(stderr, "Platform module is required.\n");
        exit(1);
    }

    if (! (load_module(platform) & (DFMOD_EP_PLATFORM|DFMOD_EP_MQUEUE)) ) {
        fprintf(stderr, "Fatal: Plaform module '%s' does not contain required entry points.\n", platform);
        exit(1);
    }
    if (sound && (!(load_module(sound) &  DFMOD_EP_MUSICSOUND)) ) {
        getplatform()->log_error("Sound module '%s' does not contain required entry points.\n", sound);
        exit(1);
    }
    if (renderer && (!(load_module(renderer) & (DFMOD_EP_TEXTURES|DFMOD_EP_SIMULOOP|DFMOD_EP_RENDERER|DFMOD_EP_KEYBOARD)))) {
        getplatform()->log_error("Renderer module '%s' does not contain required entry points.\n", renderer);
        exit(1);
    }
}
