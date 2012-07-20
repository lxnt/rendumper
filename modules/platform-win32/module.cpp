#define DFMODULE_BUILD
#include "iplatform.h"

struct implementation : public iplatform {
    implementation() {
        int ms = 1;
        while (timeBeginPeriod(ms) != TIMERR_NOERROR) ms++;
    }

    void release(void) {
        timeEndPeriod(ms);
    }

    BOOL CreateDirectory(const char* pathname, void* unused) {
        return ::CreateDirectory(pathname, NULL)
    }
    BOOL DeleteFile(const char* filename) { 
	return ::DeleteFile(filename); 
    }
    void ZeroMemory(void* dest, int len) { 
	::ZeroMemory(dest, len); 
    }
    DWORD GetTickCount() { return ::GetTickCount(); }
    char* itoa(int value, char *result, int base) { 
	return ::itoa(value, result, base); 
    }
    BOOL QueryPerformanceCounter(LARGE_INTEGER* performanceCount) {
        return ::QueryPerformanceCounter(performanceCount);
    }
    BOOL QueryPerformanceFrequency(LARGE_INTEGER* performanceCount) {
	return ::QueryPerformanceFrequency(performanceCount);
    }
    int MessageBox(HWND *dummy, const char *text, const char *caption, UINT type) {
        return ::MessageBox(dummy, text, caption, type);
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
extern "C" DECLSPEC iplatform * APIENTRY getplatform(void) {
    if (!impl)
        impl = new implementation();
    return impl;
}
