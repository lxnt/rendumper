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

extern iplatform *platform;

void _gl_fatalize(const char *fi, const int li, const char *foo) {
    GLenum err;
    bool dead = false;
    while ((err = glGetError()) != GL_NO_ERROR) {
        dead = true;
        platform->getlogr("gl")->error("%s", _gl_enum_str(err));
    }
    if (dead)
        platform->getlogr("gl")->fatal("%s:%d in %s: bye.", fi, li, foo);
}
