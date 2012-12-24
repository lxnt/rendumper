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

#if defined(__WIN32__) || defined(__CYGWIN__)
# include <windows.h>
# define _load_lib(name) LoadLibrary(name)
# define _get_sym(lib, name) GetProcAddress((HINSTANCE)lib, name)
# define _error windoze_error()
# define _release_lib(lib) FreeLibrary((HINSTANCE)lib)
const std::string _so_suffix(".dll");
const char _os_path_sep = '\\';
typedef FARPROC FOOPTR;
char windoze_errstr[4096];
static const char *windoze_error(void) {
    memset(windoze_errstr, 0, 4096);
    DWORD err = GetLastError();
    DWORD rv = FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        err,
        0,
        windoze_errstr,
        2048,
        NULL);
    if (rv == 0) {
        fprintf(stderr, "Whoa, FormatMessage() failed.\n");
        exit(1);
    }
    return windoze_errstr;
}
#else
# include <dlfcn.h>
# define _load_lib(name) dlopen(name, RTLD_NOW)
# define _error dlerror()
# define _get_sym(lib, name) dlsym(lib, name)
# define _release_lib(name) dlclose(lib)
//const char * const _so_suffix = ".so";
const char _os_path_sep = '/';
const std::string _so_suffix(".so");
typedef void * FOOPTR;
#endif

#if defined(DFMODULE_BUILD)
# error DFMODULE_BUILD defined.
#endif

#include "itypes.h"
#include "iplatform.h"

static std::string module_path("libs/");

/* blindly prepended to the soname */
static void set_modpath(const char *modpath) {
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

/* link table - pointers to pointers to entry points
   which (pointers) reside in loaded modules. */
static getplatform_t   *platform_ep[23] = { NULL };
static getmqueue_t     *mqueue_ep[23]   = { NULL };
static getrenderer_t   *renderer_ep[23] = { NULL };
static gettextures_t   *textures_ep[23] = { NULL };
static getsimuloop_t   *simuloop_ep[23] = { NULL };
static getmusicsound_t *musicsound_ep[23] = { NULL };
static getkeyboard_t   *keyboard_ep[23] = { NULL };
static const char      *module_name[23] = { NULL };

static int module_load_count = 0;
static int root_log_level = 0;

/* master dispatch table - gets replicated to the above
   and used by the executable this code gets linked to */
getplatform_t   getplatform = NULL;
getmqueue_t     getmqueue = NULL;
gettextures_t   gettextures = NULL;
getsimuloop_t   getsimuloop = NULL;
getrenderer_t   getrenderer = NULL;
getkeyboard_t   getkeyboard = NULL;
getmusicsound_t getmusicsound = NULL;

static void dump_link_table() {
    ilogger *logr = getplatform()->getlogr("glue.link.dump-table");
    if (!logr->enabled(LL_TRACE))
        return;

    for(int i = 0; i < module_load_count; i++) { // platform doesn't have deps.
        logr->trace("module %s:", module_name[i]);
        logr->trace("pointers: pl=%p mq=%p rr=%p tx=%p, sl=%p kb=%p ms=%p",
            platform_ep[i], mqueue_ep[i], renderer_ep[i], textures_ep[i], simuloop_ep[i], musicsound_ep[i], keyboard_ep[i]);

        logr->trace("*pointers: pl=%p mq=%p rr=%p tx=%p, sl=%p kb=%p ms=%p",
            platform_ep[i] ? *platform_ep[i] : NULL,
            mqueue_ep[i]   ? *mqueue_ep[i] : NULL,
            renderer_ep[i] ? *renderer_ep[i] : NULL,
            textures_ep[i] ? *textures_ep[i] : NULL,
            simuloop_ep[i] ? *simuloop_ep[i] : NULL,
            musicsound_ep[i] ? *musicsound_ep[i] : NULL,
            keyboard_ep[i] ? *keyboard_ep[i] : NULL);
    }

    logr->trace("glue final entry points: pl=%p mq=%p rr=%p tx=%p, sl=%p kb=%p ms=%p",
        getplatform, getmqueue, getrenderer, gettextures, getsimuloop, getkeyboard, getmusicsound);
}

/* returns a bitfield of which entry points were replaced.
   any error loading the so results in zero entry points replaced
   and thus return value of 0. */
static int load_module(const char *soname) {
    ilogger *logr = NULL;
    if (getplatform)
        logr = getplatform()->getlogr("glue.load_module");

    if (module_load_count > 23) {
        fprintf(stderr, "more that %d modules, you nuts?", module_load_count);
        exit(module_load_count);
    }

    std::string fname;

    fname = module_path;
    fname += soname;

    module_name[module_load_count] = strdup(fname.c_str());

    /* phew. fname += '' if fname.endswith(_so_suffix) else _so_suffix */
    if (! ((fname.size() >= _so_suffix.size())
        && (fname.compare(fname.size()-_so_suffix.size(), _so_suffix.size(), _so_suffix))== 0) )
        fname += _so_suffix;

    void *lib = _load_lib(fname.c_str());

    if (!lib) {
        if (!logr) {
            fprintf(stderr, "load_module(\"%s\"): %s\n", fname.c_str(), _error);
        } else {
            logr->error("'%s': %s\n", fname.c_str(), _error);
        }
        return 0;
    }
    /*  okay, from here we hope that we got a platform -
        first module got loaded w/o error and it should have been the platform.
        actually, enforce it. */

    int rv = 0;
    FOOPTR sym;
    dep_foo_t depfoo = NULL;

    if ((sym = _get_sym(lib, "dependencies"))) {
        depfoo = (dep_foo_t) sym;
    }

    if ((sym = _get_sym(lib, "getplatform"))) {
        rv |= DFMOD_EP_PLATFORM; getplatform = (getplatform_t) sym;
        if (!logr)
            logr = getplatform()->getlogr("glue.load_module"); // not very clean, but..
        else
            logr->fatal("second platform load attempt detected."); // also catch this
        getplatform()->logconf(NULL, root_log_level);
        logr->info("%s provides iplatform (getplatform=%p)", fname.c_str(), sym);
    } else {
        if (module_load_count == 0) {
            fputs("first module loaded must provide an iplatform\n", stderr);
            exit(8);
        }
    }

    if ((sym = _get_sym(lib, "getmqueue"))) {
        rv |= DFMOD_EP_MQUEUE; getmqueue = (getmqueue_t) sym;
        logr->info("%s provides imqueue (getmqueue=%p)", fname.c_str(), sym);
    }

    if ((sym = _get_sym(lib, "gettextures"))) {
        rv |= DFMOD_EP_TEXTURES; gettextures = (gettextures_t) sym;
        logr->info("%s provides itextures (gettextures=%p)", fname.c_str(), sym);
    }

    if ((sym = _get_sym(lib, "getrenderer"))) {
        rv |= DFMOD_EP_RENDERER; getrenderer = (getrenderer_t) sym;
        logr->info("%s provides irenderer (getrenderer=%p)", fname.c_str(), sym);
    }

    if ((sym = _get_sym(lib, "getsimuloop"))) {
        rv |= DFMOD_EP_SIMULOOP; getsimuloop = (getsimuloop_t) sym;
        logr->info("%s provides isimuloop (getsimuloop=%p)", fname.c_str(), sym);
    }

    if ((sym = _get_sym(lib, "getkeyboard"))) {
        rv |= DFMOD_EP_KEYBOARD; getkeyboard = (getkeyboard_t) sym;
        logr->info("%s provides ikeyboard (getkeyboard=%p)", fname.c_str(), sym);
    }

    if ((sym = _get_sym(lib, "getmusicsound"))) {
        rv |= DFMOD_EP_MUSICSOUND; getmusicsound = (getmusicsound_t) sym;
        logr->info("%s provides imusicsound (getmusicsound=%p)", fname.c_str(), sym);
    }

    if (depfoo) {
        depfoo(
            &platform_ep[module_load_count],
            &mqueue_ep[module_load_count],
            &renderer_ep[module_load_count],
            &textures_ep[module_load_count],
            &simuloop_ep[module_load_count],
            &musicsound_ep[module_load_count],
            &keyboard_ep[module_load_count]
        );
        logr->trace("%s requires %s%s%s%s%s%s%s", fname.c_str(),
            platform_ep[module_load_count] ? "platform " : "",
            mqueue_ep[module_load_count]   ? "mqueue " : "",
            renderer_ep[module_load_count] ? "renderer " : "",
            textures_ep[module_load_count] ? "textures " : "",
            simuloop_ep[module_load_count] ? "simuloop " : "",
            musicsound_ep[module_load_count] ? "musicsound " : "",
            keyboard_ep[module_load_count] ? "keyboard " : "");

        for (int i = 0; i < module_load_count + 1; i++) {
            if (platform_ep[i])  *platform_ep[i] = getplatform;
            if (mqueue_ep[i])    *mqueue_ep[i]   = getmqueue;
            if (renderer_ep[i])  *renderer_ep[i] = getrenderer;
            if (textures_ep[i])  *textures_ep[i] = gettextures;
            if (simuloop_ep[i])  *simuloop_ep[i] = getsimuloop;
            if (musicsound_ep[i]) *musicsound_ep[i] = getmusicsound;
            if (keyboard_ep[i])  *keyboard_ep[i] = getkeyboard;
        }
    }
    module_load_count ++;
    return rv;
}

bool lock_and_load(const char *printmode, const char *modpath, int rll) {
    root_log_level = rll;

    if (!printmode)
        printmode = getenv("DF_PRINTMODE");
    if (!printmode)
        return false;

    if (!modpath)
        modpath = getenv("DF_MODULES_PATH");
    set_modpath(modpath);

    std::string pm(printmode);
    std::string platform;

    if (pm == "ncurses") {
        platform = pm;
    } else if (pm.find("sdl2") == 0) { // str.startswith()
        platform = "sdl2";
    } else if (pm.find("sdl") == 0) {
        platform = "sdl";
    }

    std::string soname = "platform_";
    soname += platform;

    if (!load_module(soname.c_str())) {
        return false;
    }

    if (!load_module("common_code")) {
        getplatform()->getlogr("lock_and_load")->error("Failed to load common_code");
        return false;
    }

    std::string renreder = "renderer_";
    renreder += printmode;

    if (!load_module(renreder.c_str())) {
        getplatform()->getlogr("lock_and_load")->error("Failed to load renderer module %s\n", renreder.c_str());
        return false;
    }

    /* also load some non-stub sound, when we have any working */

    dump_link_table();
    return true;
}
