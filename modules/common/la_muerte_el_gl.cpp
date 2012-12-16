#include "glew.h"
#include "iplatform.h"

#define _gl_enum_case(num) case num: return #num;

const char *_gl_enum_str(GLenum num) {
    switch (num) {
        _gl_enum_case(GL_NO_ERROR)
        _gl_enum_case(GL_INVALID_ENUM)
        _gl_enum_case(GL_INVALID_VALUE)
        _gl_enum_case(GL_INVALID_OPERATION)
        _gl_enum_case(GL_INVALID_FRAMEBUFFER_OPERATION)
        _gl_enum_case(GL_OUT_OF_MEMORY)
        _gl_enum_case(GL_STACK_UNDERFLOW)
        _gl_enum_case(GL_STACK_OVERFLOW)
        default: return "unknown";
    }
}

void _gl_fatalize(const char *fi, const int li, const char *foo) {
    GLenum err;
    bool dead = false;
    while ((err = glGetError()) != GL_NO_ERROR) {
        dead = true;
        getplatform()->log_error("%s", _gl_enum_str(err));
    }
    if (dead)
        getplatform()->fatal("%s:%d in %s: bye.", fi, li, foo);
}
