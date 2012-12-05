#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <locale.h>

#include "SDL.h"
#include "SDL_thread.h"
#include "SDL_timer.h"
#include "SDL_messagebox.h"

#define DFMODULE_BUILD
#include "iplatform.h"

struct implementation : public iplatform {
    implementation() {}
    void release() {}

    BOOL CreateDirectory(const char* pathname, void* ) {
        if (mkdir(pathname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
            if (errno != EEXIST)
                log_error("mkdir(%s): %s", pathname, strerror(errno));
            return FALSE;
        } else {
            return TRUE;
        }
    }

    BOOL DeleteFile(const char* filename) { return !unlink(filename); }

    void ZeroMemory(void* dest, int len) { memset(dest, 0, len); }

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

    void log_error(const char *fmt, ...) {
        va_list ap;

        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }

    void log_info(const char *fmt, ...) {
        va_list ap;

        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
};

static implementation impl;
static bool core_init_done = false;

extern "C" DECLSPEC iplatform * APIENTRY getplatform(void) {
    if (!core_init_done) {
        setlocale(LC_ALL, ""); // why on earth do this?
        if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_NOPARACHUTE) != 0) {
            fprintf(stderr, "\nUnable to initialize SDL:  %s\n", SDL_GetError());
            exit(1);
        }
    }

    return &impl;
}

