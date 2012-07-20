#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <locale.h>

#define DFMODULE_BUILD
#include "iplatform.h"

bool osx_alert(const char *text, const char *caption, bool yesno);
bool gtk_alert(const char *text, const char *caption, bool yesno);
bool curses_alert(const char *text, const char *caption, bool yesno);

struct implementation : public iplatform {
    bool gtk_ok;
    implementation(bool gok) {
        gtk_ok = gok;
    }
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
#if defined(__APPLE__)
        ret = osx_alert(text, caption, type & MB_YESNO);
#else
# if defined(HAVE_GTK)
        if (gtk_ok)
            ret = gtk_alert(text, caption, type & MB_YESNO);
        else
# endif
# if defined(HAVE_CURSES)
        ret = curses_alert(text, caption, type & MB_YESNO);
# else
        ret = false;
# endif
#endif
        return ret ? IDOK : IDNO;
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

static implementation *impl = NULL;
static bool core_init_done = false;

extern "C" DECLSPEC iplatform * APIENTRY getplatform(void) {
    bool gtk_ok = false;
    if (!core_init_done) {
        setlocale(LC_ALL, ""); // why on earth do this?
#if defined(HAVE_GTK)
        int argc = 0;
        char *argv[] = { 0 };
        gtk_ok = gtk_init_check(&argc, &argv);
#endif
    }
    if (!impl)
        impl = new implementation(gtk_ok);
    return impl;
}

