#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#ifdef _WIN32
# undef WINDOWS_LEAN_AND_MEAN
# define WINDOWS_LEAN_AND_MEAN
# include <windows.h>
#else
# include "iplatform.h"

DWORD GetTickCount();	// returns ms since system startup
BOOL CreateDirectory(const char* pathname, void* unused);
BOOL DeleteFile(const char* filename);
BOOL QueryPerformanceCounter(LARGE_INTEGER* performanceCount);
BOOL QueryPerformanceFrequency(LARGE_INTEGER* performanceCount);
int MessageBox(HWND *dummy, const char* text, const char* caption, UINT type);
char* itoa(int value, char* result, int base);
#endif // _WIN32

#endif // _PLATFORM_H_

