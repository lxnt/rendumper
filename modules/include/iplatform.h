/* for the time being, use SDL's begin_code.h for the DECLSPEC (DFAPI here)
   which defines necessary dllimport/export stuff */
#if defined(DFMODULE_BUILD) // building the dlls
# define BUILD_SDL
#endif
#include "begin_code.h" // defines DECLSPEC
#include "end_code.h" // struct packing is not used here.
#define APIENTRY __stdcall


#if defined(WIN32)
# undef WINDOWS_LEAN_AND_MEAN
# define WINDOWS_LEAN_AND_MEAN
# include <windows.h>
#else

# define stricmp strcasecmp
# define strnicmp strncasecmp

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

# if !defined(HWND_DESKTOP)
#  define HWND_DESKTOP ((HWND)-1)
# endif

typedef int BOOL;

# if !defined(FALSE)
#  define FALSE 0
# endif
# if !defined(TRUE)
#  define TRUE 1
# endif

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
# endif

class iplatform {
  public:
    virtual void release() = 0;

    virtual void ZeroMemory(void *dest, int len) = 0; // doesn't seem to be used.
    virtual DWORD GetTickCount(void) = 0; // random.cpp and the blob.
    virtual int MessageBox(HWND *dummy, const char* text, const char* caption, UINT type) = 0;
    virtual BOOL QueryPerformanceCounter(LARGE_INTEGER* performanceCount) = 0;
    virtual BOOL QueryPerformanceFrequency(LARGE_INTEGER* performanceCount = 0;
    virtual BOOL CreateDirectory(const char* pathname, void*) = 0;
    virtual BOOL DeleteFile(const char* filename) = 0;
};

extern "C" DECLSPEC iplatform * APIENTRY getplatform(void);
