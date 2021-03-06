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

#include <vector>

#include "df_glob.h"
#include "df_buffer.h"
#include "logging.h"
#include "settings.h"

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
static pthread_mutex_t _logging_mutex = PTHREAD_MUTEX_INITIALIZER;

struct implementation : public iplatform {
    log_implementation *log_impl;

    implementation(): log_impl(NULL) {}
    void release() {}

    BOOL CreateDirectory(const char* pathname, void*) {
        if (mkdir(pathname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
            if (errno != EEXIST)
                log_impl->root_logger->error("mkdir(%s): %s", pathname, strerror(errno));
            return FALSE;
        } else {
            return TRUE;
        }
    }

    BOOL DeleteFile(const char* filename) { return !unlink(filename); }

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
            ret = true;
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

    void logconf(const char *name, int level) {
        log_impl->logconf(name, level);
    }

    ilogger *getlogr(const char *name) {
        return log_impl->getlogr(name);
    }

    NORETURN void fatal(const char *fmt, ...) {
        va_list ap;

        va_start(ap, fmt);
        log_impl->vlog_message(LL_FATAL, "platform", fmt, ap);
        va_end(ap);
        abort();
    }

    void lock_logging() {
        pthread_mutex_lock(&_logging_mutex);
    }

    void unlock_logging() {
        pthread_mutex_unlock(&_logging_mutex);
    }

    void configure_logging(const char *config) {
        log_impl->configure(config);
    }

    int bufprintf(df_buffer_t *buffer, uint32_t x, uint32_t y,
                                size_t size, uint32_t attrs, const char *fmt, ...);
    const char * const *glob(const char* pattern, const char* const exclude[],
                        const bool include_dirs, const bool include_files) {
        return df_glob(pattern, exclude, include_dirs, include_files);
    }
    void gfree(const char * const * rv) { df_gfree(rv); }

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


int implementation::bufprintf(df_buffer_t *buffer, uint32_t x, uint32_t y,
                            size_t size, uint32_t attrs, const char *fmt, ...) {

    va_list ap;
    va_start(ap, fmt);
    int rv = vbprintf(buffer, x, y, size, attrs, fmt, ap);
    va_end(ap);
    return rv;
}

static implementation impl;
static bool core_init_done = false;
static char _main_name[] = "main()";

static void ncurses_fini(void) { endwin(); }

extern "C" DFM_EXPORT iplatform * DFM_APIEP getplatform(void) {
    if (!core_init_done) {
        FILE *logfp = fopen("dfm.log", "a");
        if (!logfp) {
            fprintf(stderr, "\nlogfile open failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        impl.log_impl = new log_implementation(&impl, logfp);

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
