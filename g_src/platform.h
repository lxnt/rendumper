#ifndef _PLATFORM_H_
#define _PLATFORM_H_
#include "iplatform.h"

DWORD GetTickCount();	// returns ms since system startup
BOOL CreateDirectory(const char* pathname, void* unused);
BOOL DeleteFile(const char* filename);
BOOL QueryPerformanceCounter(LARGE_INTEGER* performanceCount);
BOOL QueryPerformanceFrequency(LARGE_INTEGER* performanceCount);
int MessageBox(HWND *dummy, const char* text, const char* caption, UINT type);
char* itoa(int value, char* result, int base);

#endif // _PLATFORM_H_

