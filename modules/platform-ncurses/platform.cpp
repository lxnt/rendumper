#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <locale.h>
#include <glob.h>

#include <curses.h>
#include <pthread.h>

#include <vector>

#define DFMODULE_BUILD
#include "iplatform.h"

namespace {

struct _thread_info {
    pthread_t thread;
    pthread_mutex_t run_mutex;
    thread_foo_t foo;
    int rv;
    char *name;
    void *data;
    bool dead;
};

static void * run_foo(void *data) {
    _thread_info *ti = (_thread_info *)data;
    pthread_mutex_lock(&ti->run_mutex);
    ti->rv = ti->foo(ti->data);
    ti->dead = true;
    pthread_mutex_unlock(&ti->run_mutex);
    return data;
}

static std::vector<_thread_info *> _thread_list;
static pthread_mutex_t _thread_list_mutex = PTHREAD_MUTEX_INITIALIZER;

struct implementation : public iplatform {
    implementation() {}
    void release() {}

    BOOL CreateDirectory(const char* pathname, void*) {
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

    int MessageBox(HWND *, const char *text, const char *caption, UINT type) {
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

    thread_t thread_create(thread_foo_t foo, const char * name, void *data) {
        int rv;
        _thread_info *ti = (_thread_info *)malloc(sizeof(_thread_info));

        ti->foo = foo;
        ti->data = data;
        ti->dead = false;
        if (name)
            ti->name = strdup(name);
        else
            ti->name = NULL;
        pthread_mutex_init(&ti->run_mutex, NULL);
        pthread_mutex_lock(&ti->run_mutex); // that is to avoid races vs thread_id()
        if ((rv = pthread_create(&(ti->thread), NULL, run_foo, ti))) {
            fprintf(stderr, "\npthread_create(): %s\n", strerror(rv));
            exit(1);
        }

        pthread_mutex_lock(&_thread_list_mutex);
        _thread_list.push_back(ti);
        pthread_mutex_unlock(&_thread_list_mutex);
        pthread_mutex_unlock(&ti->run_mutex); // let it run
        return (thread_t) ti;
    }

    void thread_join(thread_t thread, int *retval) {
        int rv;
        _thread_info *ti = (_thread_info *) thread;
        if ((rv = pthread_join(ti->thread, NULL))) {
            fprintf(stderr, "\npthread_join(): %s\n", strerror(rv));
            exit(1);
        }
        if (retval)
            *retval = ti->rv;
    }

    thread_t thread_id(void) {
        /* also sprach pthread_self(3):
            variables of type pthread_t can't portably be compared
            using the C equality  operator (==);
            use pthread_equal(3) instead. */

        pthread_t this_thread = pthread_self();
        pthread_mutex_lock(&_thread_list_mutex);
        for(size_t i = 0 ; i< _thread_list.size() ; i++)
            if (!_thread_list[i]->dead)
                if (pthread_equal(this_thread, _thread_list[i]->thread)) {
                    pthread_mutex_unlock(&_thread_list_mutex);
                    return (thread_t) (_thread_list[i]);
                }
        pthread_mutex_unlock(&_thread_list_mutex);
        return NULL; // like, some foreign thread.
    }

    void log_info(const char *fmt, ...);
    void log_error(const char *fmt, ...);
    NORETURN void fatal(const char *fmt, ...);
    const char * const *glob(const char* pattern, const char* const exceptions[],
                        const bool include_dirs, const bool include_files);
    void gfree(const char * const *);
};

static void log_sumthin(const char *fname, const char *prefix, const char *fmt, va_list ap) {
    FILE *fp;
    if ((fp = fopen(fname, "a"))) {
        fputs(prefix, fp);
        fputc(' ', fp);
        vfprintf(fp, fmt, ap);
        fputc('\n', fp);
        fclose(fp);
    }
}

void implementation::log_info(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_sumthin("dfm.log", "INFO", fmt, ap);
    va_end(ap);
}

void implementation::log_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_sumthin("dfm.log", "ERROR", fmt, ap);
    va_end(ap);
}

NORETURN void implementation::fatal(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_sumthin("dfm.log", "FATAL", fmt, ap);
    va_end(ap);
    exit(1);
}

static implementation impl;

static int glob_errfunc(const char *epath, int err) {
    impl.log_error("glob(): \"%s\": \"%s\"", epath, strerror(err));
    return 0;
}

const char * const *implementation::glob(const char* pattern, const char * const exclude[],
                    const bool include_dirs, const bool include_files) {

    glob_t g;
    size_t allocd = sizeof(char *) * 1024;
    size_t used = 0;
    char **rv = (char **) calloc(allocd, sizeof(char *));

    if (!::glob(pattern, 0, glob_errfunc, &g))
        for (size_t i = 0; i < g.gl_pathc; i++) {
            if (used == allocd - 1) {
                char **xv = (char **) calloc(2*allocd, sizeof(char *));
                if (rv) {
                    memmove(xv, rv, sizeof(char *) * allocd);
                    free(rv);
                }
                rv = xv;
            }

            struct stat cstat;
            stat(g.gl_pathv[i], &cstat);

            if (S_ISREG(cstat.st_mode) && !include_files)
                continue;

            if (S_ISDIR(cstat.st_mode) && !include_dirs)
                continue;

            char *basename = strrchr(g.gl_pathv[i], '/');
            if (!basename)
                basename = g.gl_pathv[i];

            if (exclude)
                for (const char * const *e = exclude; *e != NULL; e++)
                    if (!strcmp(basename, *e))
                        continue;

            rv[used++] = strdup(basename);
        }
    globfree(&g);
    return rv;
}

void implementation::gfree(const char * const * rv) {
    const char * const *whoa = rv;
    while (*whoa)
        free((void *) *(whoa++));
    free((void *)rv);
}

static bool core_init_done = false;
static char _main_name[] = "main()";

static void ncurses_fini(void) { endwin(); }

extern "C" DECLSPEC iplatform * APIENTRY getplatform(void) {
    if (!core_init_done) {
        int rv = pthread_mutex_lock(&_thread_list_mutex);
        _thread_info *mt = new _thread_info();
        mt->thread = pthread_self();
        mt->foo = NULL;
        mt->rv = 0;
        mt->name = _main_name;
        mt->data = NULL;
        mt->dead = false;
        _thread_list.push_back(mt);
        rv = pthread_mutex_unlock(&_thread_list_mutex);

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
} /* ns */
