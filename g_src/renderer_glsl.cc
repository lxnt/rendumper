/* Okay. 
    So, no rewriting python code in C++.
    And not a line of C++ above absolutely necessary.

    A basic interface to the DF, and then C bindings to Python.


What this renderer does, and where python comes in.

Python: initialization, finalization, reshape, texture management.
C++ : remaining cruft.


*/

#include "Python.h"

#include <sys/mman.h>

#include "GL/glew.h"

#define NO_SDL_GLEXT // ahem. do we need the glew after all ?
#include "SDL.h"
#include "SDL_video.h"
#include "SDL_opengl.h"


#include "renderer_glsl.h"
#include "enabler.h"

struct glsl_private {
    SDL_Window *window;
    SDL_GLContext context;
    
    GLuint gl_program;         // set by python code. 
    
    PyObject *py_gl_init;      // def gl_init(): pass # initialize an existing context
    PyObject *py_gl_fini;      // def gl_fini(): pass # release all GL object we know of
    PyObject *py_zoom;         // def zoom(cmd): pass    # handle zoom and fullscreen toggle
    PyObject *py_resize;       // def resize(x, y): pass # handle window resize. has to call dfr.grid_resize().
    PyObject *py_uniforms;     // def uniforms(): pass # update uniforms, expect glUseProgram already done.
        
    PyObject *load_textures;
   
    void gps_ulod_resize(int w, int h) {
        fprintf(stderr, "gps_ulod_resize(%d, %d)\n", w, h);
        ulod_resize(w, h);
        gps.resize(w, h);
    }

    struct _bo_offset {
        unsigned char *head;
        unsigned w, h;
        
        // offsets:
        unsigned screen;     // uchar[4]
        unsigned texpos;     // long
        unsigned addcolor;   // long
        unsigned grayscale;  // uchar
        unsigned cf;         // uchar
        unsigned cbr;        // uchar
        unsigned grid;       // ushort[2]
    } bo;
    
    void ulod_resize(int w, int h) {
        /* max window size = max map size = 768*768*unit_sz which is 16.
           thus we may need a maximum of 9Mb. Just mmap it all and be
           done with it. No fuss with page-aligning malloc()'s result.
           Resizes, ha. Just adjust offsets. */
        if (!bo.head) {
            bo.head = (unsigned char *) mmap(NULL, 768*768*16+4096, 
                                PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE,-1, 0);
            if (bo.head == MAP_FAILED) {
                perror("mmap");
                exit(1);
            }
        }
        bo.w = w; bo.h = h;

        bo.screen                       = w*h*(0);
        bo.texpos                       = w*h*(4);
        bo.addcolor                     = w*h*(4+4);
        bo.grayscale                    = w*h*(4+4+1);
        bo.cf                           = w*h*(4+4+1+1);
        bo.cbr                          = w*h*(4+4+1+1+1);
        bo.grid                         = w*h*(4+4+1+1+1+1);

        gps.screen                      = (unsigned char *) ( bo.head + bo.screen );
        gps.screentexpos                = (          long *) ( bo.head + bo.texpos );
        gps.screentexpos_addcolor       = (          char *) ( bo.head + bo.addcolor );
        gps.screentexpos_grayscale      = (unsigned char *) ( bo.head + bo.grayscale );
        gps.screentexpos_cf             = (unsigned char *) ( bo.head + bo.cf );
        gps.screentexpos_cbr            = (unsigned char *) ( bo.head + bo.cbr );

        // screen is memset(0) in graphicst::erasescreen
        // grid is initialized below.
        // memset the rest
        memset(gps.screentexpos, 0, w*h*8); 

        /* init grid. it's two shorts describing the tile position 
           in the viewport just like fgtesbed (that is, in DF grid coords) */
        for (int i=0; i <w*h; i++)
            *((short *)(bo.head+bo.grid+i*2)) = i % w, *((short *)(bo.head+bo.grid+i*2 +1)) = i / w;

        gps.resize(w,h);
    }
    glsl_private() { bo.head = NULL; gl_program = 0;}
};

static glsl_private *dfr_self;

static PyObject*
dfr_grid_resize(PyObject *self, PyObject *args) {
    int w, h;
    if(!PyArg_ParseTuple(args, "ii", &w, &h))
        return NULL;
    
    dfr_self->ulod_resize(w, h);
    Py_RETURN_NONE;
}

static PyObject*
dfr_set_program(PyObject *self, PyObject *args) {
    unsigned p;
    if(!PyArg_ParseTuple(args, "I", &p))
        return NULL;
    
    dfr_self->gl_program = p;
    Py_RETURN_NONE;
}

static PyObject*
dfr_set_callback(PyObject *self, PyObject *args) {
    char *name;
    PyObject *candidate;
    if(!PyArg_ParseTuple(args, "sO", &name, &candidate))
        return NULL;
    
    if (!(candidate && PyCallable_Check(candidate))) {
        PyErr_SetString(PyExc_TypeError, "candidate callback is not callable");
        return NULL;
    }
    
    if (!strcmp("gl_init", name)) {
        Py_INCREF(candidate);
        dfr_self->py_gl_init = candidate;
        Py_RETURN_NONE;
    } 
    if (!strcmp("gl_fini", name)) {
        Py_INCREF(candidate);
        dfr_self->py_gl_fini = candidate;
        Py_RETURN_NONE;
    } 
    if (!strcmp("zoom", name)) {
        Py_INCREF(candidate);
        dfr_self->py_zoom = candidate;
        Py_RETURN_NONE;
    } 
    if (!strcmp("resize", name)) {
        Py_INCREF(candidate);
        dfr_self->py_resize = candidate;
        Py_RETURN_NONE;
    }
    if (!strcmp("uniforms", name)) {
        Py_INCREF(candidate);
        dfr_self->py_uniforms = candidate;
        Py_RETURN_NONE;
    } 
    PyErr_Format(PyExc_NameError ,"unknown callback name '%'", name);
    return NULL;
}

static int
_add_uint_to_dict(PyObject *dick, const char *key, uint val) {
    PyObject *pyval;
    int rv = -1;
    pyval = Py_BuildValue("I", val);
    if (pyval) {
        if ( PyDict_SetItemString(dick, key, pyval) == 0 )
            rv = 0;
        else {
            Py_DECREF(dick);
            rv = -1;
        }
        Py_DECREF(pyval);
    }
    return rv;
}

static PyObject*
dfr_ulod_info(PyObject *self, PyObject *args) {
    PyObject *dick;
    if (!(dick = PyDict_New()))
        return NULL;
    
    if (_add_uint_to_dict(dick, "head", (unsigned) dfr_self->bo.head)) return NULL; // 64bit unsafe.
    if (_add_uint_to_dict(dick, "w", dfr_self->bo.w)) return NULL;
    if (_add_uint_to_dict(dick, "h", dfr_self->bo.h)) return NULL;
    if (_add_uint_to_dict(dick, "screen", dfr_self->bo.screen)) return NULL;
    if (_add_uint_to_dict(dick, "texpos", dfr_self->bo.texpos)) return NULL;
    if (_add_uint_to_dict(dick, "addcolor", dfr_self->bo.addcolor)) return NULL;
    if (_add_uint_to_dict(dick, "grayscale", dfr_self->bo.grayscale)) return NULL;
    if (_add_uint_to_dict(dick, "cf", dfr_self->bo.cf)) return NULL;
    if (_add_uint_to_dict(dick, "cbr", dfr_self->bo.cbr)) return NULL;
    if (_add_uint_to_dict(dick, "grid", dfr_self->bo.grid)) return NULL;
    
    return dick;
}

static PyMethodDef DfrMethods[] = {
    {"grid_resize", dfr_grid_resize, METH_VARARGS,
     "Resize the grid and gps array, if necessary."},
    {"ulod_info", dfr_ulod_info, METH_NOARGS,
     "Return struct _bo_offset as a dict." },
    {"set_callback", dfr_set_callback, METH_VARARGS,
     "Set a callback (name, callable); name one of gl_init, gl_fini, zoom, resize."},
    {"set_program", dfr_set_program, METH_VARARGS,
     "Set the gl_program (shader) name"},

/*    {"", dfr_, METH_VARARGS,
     ""},
*/
     {NULL, NULL, 0, NULL}
};

static PyModuleDef DfrModule = {
    PyModuleDef_HEAD_INIT, "dfr", NULL, -1, DfrMethods,
    NULL, NULL, NULL, NULL
};

static PyObject*
PyInit_dfr(void) {
    return PyModule_Create(&DfrModule);
}

static void python_init(void) {
    /* fiddle with env because gdb is compiled with py2.7 */
    char *dfp = getenv("DFPYPATH");
    if (!dfp || (strlen(dfp) == 0)) {
        fprintf(stderr, "DFPYPATH envvar is required.\n");
        exit(1);
    }
    setenv("PYTHONPATH", dfp,1); 
    
    PyImport_AppendInittab("dfr", &PyInit_dfr);
    Py_Initialize();
    

    PyRun_SimpleString("import sys\nprint(repr(sys.path))\n");
    
    PyObject *pModName = PyUnicode_FromString("renderer");
    PyObject *pRendererModule = PyImport_Import(pModName);
    Py_DECREF(pModName);
    if (pRendererModule == NULL) {
        PyErr_Print();
        fprintf(stderr, "Python renderer module load failed.\n");
        exit(1);
    }
    /* expect the renderer code to set up needed callbacks */
}

static void _callcallback(PyObject *cb) {
    PyObject *pRetVal = PyObject_CallObject(cb, NULL);
    if (!pRetVal) {
        PyErr_Print();
        exit(1);
    }
    Py_DECREF(pRetVal);    
}

void renderer_glsl::render() {
    glUseProgram(self->gl_program);
    _callcallback(self->py_uniforms);
    glDrawArrays(GL_POINTS, 0, self->bo.w*self->bo.h);
    SDL_GL_SwapWindow(self->window);
}

void renderer_glsl::zoom(zoom_commands cmd) {
    PyObject *pArgs = PyTuple_New(1);
    PyObject *pValue = PyLong_FromLong(cmd);
    PyTuple_SetItem(pArgs, 0, pValue);
    PyObject *pRetVal = PyObject_CallObject(self->py_zoom, pArgs);
    Py_DECREF(pArgs);
    if (!pRetVal) {
        PyErr_Print();
        exit(1);
    }
    Py_DECREF(pRetVal);
}

void renderer_glsl::resize(int w, int h) {
    PyObject *pArgs = PyTuple_New(2);
    PyObject *pValueW = PyLong_FromLong(w);
    PyObject *pValueH = PyLong_FromLong(h);
    PyTuple_SetItem(pArgs, 0, pValueW);
    PyTuple_SetItem(pArgs, 1, pValueH);
    PyObject *pRetVal = PyObject_CallObject(self->py_resize, pArgs);
    Py_DECREF(pArgs);
    if (!pRetVal) {
        PyErr_Print();
        exit(1);
    }
    Py_DECREF(pRetVal);    
}

void renderer_glsl::grid_resize(int w, int h) {
    fprintf(stderr, "renderer_glsl::grid_resize(%d, %d)\n", w, h);
    self->gps_ulod_resize(w, h);
}

renderer_glsl::renderer_glsl() {
    // well. we're here when we've got SDL_InitSubSystem(VIDEO) done. that's it.
    self = dfr_self = new glsl_private();
    
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    if (true) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    } else {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
        //SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    }

    self->window = SDL_CreateWindow("dfsl", 
                        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 800,
                        SDL_WINDOW_SHOWN|SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);

    self->context = SDL_GL_CreateContext(self->window);
    int gl_ma, gl_mi;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &gl_ma),
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &gl_mi);

    // clean error state so that pyopengl doesn't choke on it
    GLenum err;
    if (GLEW_OK != (err = glewInit())) {
        fprintf(stderr, "glewInit(): %s\n", glewGetErrorString(err));
        exit(1);
    }
    fprintf(stderr, "GLEW %s\n", glewGetString(GLEW_VERSION));
    GLenum glerr;
    while (0 != (glerr = glGetError()))
        fprintf(stderr, "glewInit glGetError(): %d\n", glerr);
    int w,h;
    SDL_GetWindowSize(self->window, &w, &h);
    python_init();
    _callcallback(self->py_gl_init);
    resize(w,h); // init grid and other shit
    
}
bool renderer_glsl::get_mouse_coords(int &x, int &y) {
    if ( SDL_GetWindowFlags(self->window) && SDL_WINDOW_MOUSE_FOCUS) {
        SDL_GetMouseState(&x, &y);
        return true;
    }
    return false;
}

renderer_glsl::~renderer_glsl() { }
