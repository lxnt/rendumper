#if !defined(LA_MUERTE_EL_GL_H)
#define LA_MUERTE_EL_GL_H

void _gl_fatalize(const char *, const int, const char *);

#define GL_DEAD_YET(where) _gl_fatalize(__FILE__, __LINE__, __func__)

#endif
