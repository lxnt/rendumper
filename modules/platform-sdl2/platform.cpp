#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <locale.h>

#include <string>
#include <unordered_map>

#include "df_glob.h"
#include "df_buffer.h"
#include "logging.h"
#include "settings.h"

#include "SDL.h"
#include "SDL_thread.h"
#include "SDL_mutex.h"
#include "SDL_timer.h"
#include "SDL_messagebox.h"

#define DFMODULE_BUILD
#include "iplatform.h"

iplatform *platform; // for the mqueue

namespace {

SDL_mutex *_logging_mutex;

struct implementation : public iplatform {
    log_implementation log_impl;

    implementation(): log_impl(this, stderr) {}
    void release() {}

    BOOL CreateDirectory(const char* pathname, void *) {
#if defined(__WIN32__) || defined(__CYGWIN__)
        return ::CreateDirectory(pathname, NULL);
#else
        if (mkdir(pathname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
            if (errno != EEXIST)
                log_impl.root_logger->error("mkdir(%s): %s", pathname, strerror(errno));
            return FALSE;
        } else {
            return TRUE;
        }
#endif
    }

    BOOL DeleteFile(const char* filename) { return !unlink(filename); }

    DWORD GetTickCount() { return SDL_GetTicks(); }

    BOOL QueryPerformanceCounter(LARGE_INTEGER* performanceCount) {
        performanceCount->QuadPart = SDL_GetPerformanceCounter();
        return TRUE;
    }

    BOOL QueryPerformanceFrequency(LARGE_INTEGER* performanceCount) {
        performanceCount->QuadPart = SDL_GetPerformanceFrequency();
        return TRUE;
    }

    int MessageBox(HWND *, const char *text, const char *caption, UINT /* type */) {
        Uint32 flags = 0;
#if 0
    /* this stuff isn't even implemented in SDL yet.
        I don't know what it's supposed to mean either. */
        if (type & MB_ICONQUESTION)
            flags &= SDL_MESSAGEBOX_INFORMATION;

        if (type & MB_ICONEXCLAMATION)
            flags &= SDL_MESSAGEBOX_ERROR;
#endif
        return SDL_ShowSimpleMessageBox(flags, caption, text, NULL);
    }

    thread_t thread_create(thread_foo_t foo, const char *name, void *data) {
        return (thread_t ) SDL_CreateThread(foo, name, data);
    }

    void thread_join(thread_t thread, int *retval) {
        int rv;
        SDL_WaitThread((SDL_Thread *) thread, &rv);
        if (retval)
            *retval = rv;
    }

    thread_t thread_id(void) {
        return (thread_t ) SDL_ThreadID();
    }

    void logconf(const char *name, int level) {
        log_impl.logconf(name, level);
    }

    ilogger *getlogr(const char *name) {
        return log_impl.getlogr(name);
    }

    NORETURN void fatal(const char *fmt, ...) {
        va_list ap;

        va_start(ap, fmt);
        log_impl.vlog_message(LL_FATAL, "platform", fmt, ap);
        va_end(ap);
        abort();
    }

    void lock_logging() { SDL_LockMutex(_logging_mutex); }
    void unlock_logging() { SDL_UnlockMutex(_logging_mutex); }
    void configure_logging(const char *config) { log_impl.configure(config); }

    int bufprintf(df_buffer_t *buffer, uint32_t x, uint32_t y,
                                size_t size, uint32_t attrs, const char *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        int rv = vbprintf(buffer, x, y, size, attrs, fmt, ap);
        va_end(ap);
        return rv;
    }

    const char * const *glob(const char* pattern, const char * const exclude[],
                    const bool include_dirs, const bool include_files) {
        return df_glob(pattern, exclude, include_dirs, include_files);
    }

    void gfree(const char * const *rv) { df_gfree(rv); }

    void set_setting(const char* filename, const char *name, const char *value) {
        cc_set_setting(filename, name, value);
    }
    const char* get_setting(const char *name) {
        return cc_get_setting(name, NULL);
    }
    const char* get_setting(const char *name, const char *fallback) {
        return cc_get_setting(name, fallback);
    }
    const ansi_colors_t *get_colors(void) {
        return cc_get_colors();
    }
};

static implementation impl;
static bool core_init_done = false;

extern "C" DFM_EXPORT iplatform * DFM_APIEP getplatform(void) {
    if (!core_init_done) {
        setlocale(LC_ALL, ""); // no idea if that's the right thing to do on windoze
        if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_NOPARACHUTE) != 0)
            impl.fatal("Unable to initialize SDL:  %s", SDL_GetError());
        _logging_mutex = SDL_CreateMutex();
        platform = &impl;
        core_init_done = true;
    }

    return &impl;
}
} /* ns */
