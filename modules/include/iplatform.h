#if !defined(IPLATFORM_H)
#define IPLATFORM_H

/* for the time being, use SDL's begin_code.h for the DECLSPEC (DFAPI here)
   which defines necessary dllimport/export stuff */

#include <cstddef>
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
    IDYES,
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
    };
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
    virtual void log_error(const char *, ...) = 0;
    virtual void log_info(const char *, ...) = 0;
};


#if defined (DFMODULE_BUILD) || defined(DFMODULE_IMPLICIT_LINK)
extern "C" DECLSPEC iplatform * APIENTRY getplatform(void);
#else // using glue and runtime loading.
extern "C" DECLSPEC iplatform * APIENTRY (*getplatform)(void);
#endif

/* Interthread message queues */

typedef size_t imqd_t;
enum mq_errors {
    IMQ_OK               = 0,
    IMQ_EXIST            = -17,    /* open() with maxmsg > 0  and queue already exists. */
    IMQ_NOENT            = -2,     /* open() with maxmsg == 0  and queue doesn't exist. */
    IMQ_TIMEDOUT         = -110,   /* ETIMEDOUT or timeout == 0 and no messages/no space in q. */
    IMQ_BADF             = -8,     /* EBADF bogus imqd supplied. */
    IMQ_INVAL            = -22,    /* EINVAL : null name, null buf or zero len */
    IMQ_CLOWNS           = -100500 /* unknown error, most likely from the implementation's backend */
};

/* interface loosely modelled on POSIX mqueues */

struct imqueue {
    virtual void release() = 0;

    /* max_messages == 0: open an existing queue or IMQ_NOENT, increment refcount
       max_messages >  0: create a new queue or IMQ_EXIST */
    virtual imqd_t open(const char *name, int max_messages) = 0;

    /* close: decrement refcount, destroy if it reaches 0 and queue is unlinked. */
    virtual int close(imqd_t qd) = 0;

    /* unlink, destroy if refcount == 0. Another queue with the same name can be created. */
    virtual int unlink(const char *name) = 0;

    /* timeout is in milliseconds.
        timeout > 0:    wait
        timeout == 0:   return at once
        timeout < 0:    block. */

    /* send a message contained in (buf, len), sender gives up ownership of the buffer. */
    virtual int send(imqd_t qd, void *buf, size_t len, int timeout) = 0;

    /* sender retains ownership of the buffer, implementation copies it. */
    virtual int copy(imqd_t qd, void *buf, size_t len, int timeout) = 0;

    /* incoming message buffer is in (*buf, *len); receiver owns it */
    virtual int recv(imqd_t qd, void **buf, size_t *len, int timeout) = 0;

    /* use this to free copied buffers */
    virtual void free(void *buf) = 0;
};

#if defined (DFMODULE_BUILD) || defined(DFMODULE_IMPLICIT_LINK)
extern "C" DECLSPEC imqueue * APIENTRY getmqueue(void);
#else // using glue and runtime loading.
extern "C" DECLSPEC imqueue * APIENTRY (*getmqueue)(void);
#endif

#endif
