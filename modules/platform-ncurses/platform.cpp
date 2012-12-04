#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <locale.h>

#include <curses.h>
#include <pthread.h>

#define DFMODULE_BUILD
#include "iplatform.h"


struct _thread_info {
    pthread_t thread;
    thread_foo_t foo;
    int rv;
    void *data;
};

static void * run_foo(void *data) {
    _thread_info *ti = (_thread_info *)data;
    ti->rv = ti->foo(ti->data);
    return data;
}

struct implementation : public iplatform {
    implementation() {}
    void release() {}

    BOOL CreateDirectory(const char* pathname, void* unused) {
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

    /* Returns milliseconds since 1970
     * Wraps every 24 days (assuming 32-bit signed dwords)
     */
    DWORD GetTickCount() {
      struct timeval tp;
      gettimeofday(&tp, NULL);
      return (tp.tv_sec * 1000) + (tp.tv_usec / 1000);
    }

    // Fills performanceCount with microseconds passed since 1970
    // Wraps in twenty-nine thousand years or so
    BOOL QueryPerformanceCounter(LARGE_INTEGER* performanceCount) {
        struct timeval tp;
        gettimeofday(&tp, NULL);
        performanceCount->QuadPart = ((long long)tp.tv_sec * 1000000) + tp.tv_usec;
        return TRUE;
    }

    BOOL QueryPerformanceFrequency(LARGE_INTEGER* performanceCount) {
        /* A constant, 10^6, as we give microseconds since 1970 in
        * QueryPerformanceCounter. */
        performanceCount->QuadPart = 1000000;
        return TRUE;
    }

    int MessageBox(HWND *dummy, const char *text, const char *caption, UINT type) {
        bool ret;
        werase(stdscr);
        wattrset(stdscr, A_NORMAL | COLOR_PAIR(1));

        mvwaddstr(stdscr, 0, 5, caption);
        mvwaddstr(stdscr, 2, 2, text);
        nodelay(stdscr, false);
        if (type & MB_YESNO) {
            mvwaddstr(stdscr, 5, 0, "Press 'y' or 'n'.");
            refresh();
            while (1) {
                char i = wgetch(stdscr);
                if (i == 'y') {
                    ret = true;
                    break;
                }
                else if (i == 'n') {
                    ret = false;
                break;
                }
            }
        } else {
            mvwaddstr(stdscr, 5, 0, "Press any key to continue.");
            refresh();
            wgetch(stdscr);
        }
        nodelay(stdscr, -1);

        return ret ? IDOK : IDNO;
    }

    thread_t thread_create(thread_foo_t foo, const char *name, void *data) {
        _thread_info *rv = (_thread_info *)malloc(sizeof(_thread_info));
        rv->foo = foo;
        rv->data = data;
        if (!pthread_create(&(rv->thread), NULL, run_foo, rv)) {
            fprintf(stderr, "\npthread_create(): %s\n", strerror(errno));
            exit(1);
        }
        return (thread_t) rv;
    }

    void thread_join(thread_t thread, int *retval) {
        _thread_info *ti = (_thread_info *) thread;
        if (!pthread_join(ti->thread, NULL)) {
            fprintf(stderr, "\npthread_join(): %s\n", strerror(errno));
            exit(1);
        }
        if (retval)
            *retval = ti->rv;
    }

    void log_error(const char *fmt, ...) {
        va_list ap;
        int rv;

        va_start(ap, fmt);
        rv = vfprintf(stderr, fmt, ap);
        va_end(ap);
    }

    void log_info(const char *fmt, ...) {
        va_list ap;
        int rv;

        va_start(ap, fmt);
        rv = vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
};

static implementation impl;
static bool core_init_done = false;

static void ncurses_fini(void) { endwin(); }

extern "C" DECLSPEC iplatform * APIENTRY getplatform(void) {
    if (!core_init_done) {
        core_init_done = true;
        setlocale(LC_ALL, "");
        WINDOW *new_window = initscr();
        if (!new_window) {
            fprintf(stderr, "\nncurses initialization failed.\n");
            exit(EXIT_FAILURE);
        }
        raw();
        noecho();
        keypad(stdscr, true);
        nodelay(stdscr, true);
        set_escdelay(25);
        curs_set(0);
    #if 0
        mmask_t dummy;
        mousemask(ALL_MOUSE_EVENTS, &dummy);
    #endif
        start_color();
        init_pair(1, COLOR_WHITE, COLOR_BLACK);

        atexit(ncurses_fini);
    }

    return &impl;
}

