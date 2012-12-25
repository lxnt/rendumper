#include "iplatform.h"
#include <string>
#include <algorithm>

DWORD GetTickCount() {
    return getplatform()->GetTickCount();
}
BOOL CreateDirectory(const char* pathname, void* unused) {
    return getplatform()->CreateDirectory(pathname, unused);
}
BOOL DeleteFile(const char* filename) {
    return getplatform()->DeleteFile(filename);
}
BOOL QueryPerformanceCounter(LARGE_INTEGER* performanceCount) {
    return getplatform()->QueryPerformanceCounter(performanceCount);
}
BOOL QueryPerformanceFrequency(LARGE_INTEGER* performanceCount) {
    return getplatform()->QueryPerformanceFrequency(performanceCount);
}
int MessageBox(HWND *dummy, const char* text, const char* caption, UINT type) {
    return getplatform()->MessageBox(dummy, text, caption, type);
}

char* itoa(int value, char* result, int base)
{
  // check that the base is valid
  if (base < 2 || base > 16) { *result = 0; return result; }
	
  char* out = result;
  int quot = value;
	
  do
    {
      *out = "0123456789abcdef"[ /*std::*/abs(quot % base) ];
      ++out;
      quot /= base;
    }
  while (quot);
	
  if (value < 0) *out++ = '-';
	
  std::reverse(result, out);
  *out = 0;
  return result;
}