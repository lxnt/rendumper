#if !defined(IPLATFORM_H)
#define IPLATFORM_H

#include <cstddef>
#include <cstdint>
#include "ideclspec.h"

#if defined(WIN32)
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

typedef int HANDLE;
typedef HANDLE HINSTANCE;
typedef HANDLE HWND;
typedef HANDLE HDC;
typedef HANDLE HGLRC;
typedef int BOOL;
typedef unsigned short WORD;
typedef uint32_t DWORD;
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

struct iplatform {
    virtual void release() = 0;

    /* Is not used, at least on Linux. */
    virtual void ZeroMemory(void *dest, int len) = 0;

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
    virtual void log_info(const char *, ...) = 0;
    virtual void log_error(const char *, ...) = 0;
    virtual NORETURN void fatal(const char *, ...) = 0;

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


#if defined (DFMODULE_BUILD) || defined(DFMODULE_IMPLICIT_LINK)
extern "C" DECLSPEC iplatform * APIENTRY getplatform(void);
#else // using glue and runtime loading.
extern "C" DECLSPEC iplatform * APIENTRY (*getplatform)(void);
#endif

#endif
