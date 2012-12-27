#if !defined(IPLATFORM_H)
#define IPLATFORM_H

#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include "itypes.h"

#if defined(__WIN32__) || defined(__CYGWIN__)
# undef WINDOWS_LEAN_AND_MEAN
# define WINDOWS_LEAN_AND_MEAN
# include <windows.h>
#else
# define stricmp strcasecmp
# define strnicmp strncasecmp
# if !defined(HWND_DESKTOP)
#  define HWND_DESKTOP ((HWND)-1)
# endif
# if !defined(FALSE)
#  define FALSE 0
# endif
# if !defined(TRUE)
#  define TRUE 1
# endif

enum {
    // NOTE: These probably don't match Windows values.
    MB_OK    = 0x01,
    MB_YESNO = 0x02,
    MB_ICONQUESTION    = 0x10,
    MB_ICONEXCLAMATION = 0x20,

    IDOK = 1,
    IDNO,
    IDYES
};

typedef void *HANDLE;
typedef HANDLE HINSTANCE;
typedef HANDLE HWND;
typedef HANDLE HDC;
typedef HANDLE HGLRC;
typedef int BOOL;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef unsigned int UINT;
typedef short SHORT;
typedef long LONG;
typedef long long LONGLONG;
typedef WORD WPARAM;
typedef DWORD LPARAM;

typedef struct {
    LONG x;
    LONG y;
} POINT;

typedef union {
    struct {
        DWORD LowPart;
        LONG HighPart;
    } u;
    LONGLONG QuadPart;
} LARGE_INTEGER;

typedef struct {
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    POINT pt;
} MSG;

# endif /* WIN32 */

# if defined(__GNUC__) and !defined(NORETURN)
#  define NORETURN  __attribute__((noreturn))
# endif
# if !defined(NORETURN)
#  define NORETURN
# endif

typedef struct _unidentified_flying_struct *thread_t;
typedef int (*thread_foo_t)(void *);

#define LL_NONE     0
#define LL_TRACE    1
#define LL_INFO     2
#define LL_WARN     3
#define LL_ERROR    4
#define LL_FATAL    5

struct ilogger {
    virtual bool enabled(const int) = 0;

    virtual void trace(const char *, ...) = 0;
    virtual void info(const char *, ...) = 0;
    virtual void warn(const char *, ...) = 0;
    virtual void error(const char *, ...) = 0;

    virtual NORETURN void fatal(const char *, ...) = 0;
};

struct iplatform {
    virtual void release() = 0;

    /* Retrieves the number of milliseconds that have elapsed since
       the system was started, up to 49.7 days. */
    virtual DWORD GetTickCount(void) = 0; // random.cpp and the blob.

    /* Displays a modal dialog box that contains a system icon, a set of buttons,
       and a brief application-specific message, such as status or error information.
       The message box returns an integer value that indicates which button the user clicked. */
    virtual int MessageBox(HWND *dummy, const char* text, const char* caption, UINT type) = 0;

    /* Retrieves the current value of the high-resolution performance counter.  */
    virtual BOOL QueryPerformanceCounter(LARGE_INTEGER* performanceCount) = 0;

    /* Retrieves the frequency of the high-resolution performance counter, if one exists.
       The frequency cannot change while the system is running. */
    virtual BOOL QueryPerformanceFrequency(LARGE_INTEGER* performanceCount) = 0;
    virtual BOOL CreateDirectory(const char* pathname, void* unused) = 0;
    virtual BOOL DeleteFile(const char* filename) = 0;

    /* Minimal amount of stuff needed for thread control. */
    virtual thread_t thread_create(thread_foo_t foo, const char *name, void *data) = 0;
    virtual void thread_join(thread_t thread, int *rv) = 0;
    virtual thread_t thread_id(void) = 0;

    /* Logging ended up here too. */
    virtual void logconf(const char *name, int level) = 0;
    virtual ilogger *getlogr(const char *) = 0;
    virtual NORETURN void fatal(const char *, ...) = 0;
    virtual void lock_logging() = 0;
    virtual void unlock_logging() = 0;

    /*  Puts a string into the buffer. string gets truncated at size characters,
        or at the buffer border; no wrapping.

        attrs are:
            lil endian:
                (br<<24) | (bg<<16) | (fg<<8)
            big endian:
                (fg<<16) | (bg<<8 )| (br)

        return value:
            characters written, that is min(final_strlen, size, dimx-x)
            or -1.
    */
    virtual int bufprintf(df_buffer_t *buffer, uint32_t x, uint32_t y,
                                size_t size, uint32_t attrs, const char *fmt, ...) = 0;

    /* File/directory finder.
        parameters:
            pattern - a glob(3) pattern
            exceptions - NULL-terminated array of basenames to ignore.
            include_files - include S_IFREGs in results (sys/stat.h).
            include_dirs  - include S_IFDIRs in results.

        return value:
            A pointer to a NULL-terminated array of matching paths.
            Errors are not reported. NULLs are never returned.

            Always use the gfree method to free the return value. */
    virtual const char * const *glob(const char* pattern, const char* const exceptions[],
                        const bool include_dirs, const bool include_files) = 0;

    /* Use this to free the return value. ignores NULLs*/
    virtual void gfree(const char * const *) = 0;
};


#if defined (DFMODULE_BUILD)
extern "C" DFM_EXPORT iplatform * DFM_APIEP getplatform(void);
#else // using glue and runtime loading.
extern "C" iplatform * DFM_APIEP (*getplatform)(void);
#endif

#endif
