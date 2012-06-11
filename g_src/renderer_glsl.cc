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

#define NO_SDL_GLEXT 
#include "SDL.h"
#include "SDL_video.h"
#include "SDL_opengl.h"


#include "renderer_glsl.h"
#include "enabler.h"

struct glsl_private {
    std::string libdir;        // where the .so-s are, for pygame2 and pythonpath

    SDL_Window *window;
    SDL_GLContext context;

    PyObject *py_gl_init;      // def gl_init(): pass # initialize an existing context
    PyObject *py_gl_fini;      // def gl_fini(): pass # release all GL object we know of
    PyObject *py_zoom;         // def zoom(cmd): pass    # handle zoom and fullscreen toggle
    PyObject *py_resize;       // def resize(x, y): pass # handle window resize. has to call dfr.grid_resize().
    PyObject *py_render;       // def render(): pass # do a frame
    PyObject *py_accept_textures; // def accept_textures(raws): pass
   
    void gps_ulod_resize(int w, int h) {
        fprintf(stderr, "gps_ulod_resize(%d, %d)\n", w, h);
        ulod_resize(w, h);
        gps.resize(w, h);
    }

    struct _bo_offset {
        unsigned char *head;
        unsigned w, h;
        unsigned used;
        
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
                                PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
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

        bo.used                         = w*h*16;
        // screen is memset(0) in graphicst::erasescreen
        // grid is initialized below.
        // memset the rest
        memset(gps.screentexpos, 0, w*h*8); 

        /* init grid. it's two shorts describing the tile position 
           in the viewport in NDC coord system. 
           However, df's screen and friends are row-wise, 
           that is, tileat(x,y) = x*w + y. This is accounted here for. */
        for (int i=0; i <w*h; i++)
            *((short *)(bo.head+bo.grid+i*4)) = i / h, *((short *)(bo.head+bo.grid+i*4 +2)) = i % h;

        gps.resize(w,h);
    }
    glsl_private() { bo.head = NULL; libdir=""; step=0; frame=0; }
    int step;
    int frame;
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
    if (!strcmp("render", name)) {
        Py_INCREF(candidate);
        dfr_self->py_render = candidate;
        Py_RETURN_NONE;
    } 
    if (!strcmp("accept_textures", name)) {
        Py_INCREF(candidate);
        dfr_self->py_accept_textures = candidate;
        Py_RETURN_NONE;
    } 
    PyErr_Format(PyExc_NameError ,"unknown callback name '%'", name);
    return NULL;
}

static PyObject*
dfr_ccolor(PyObject *self, PyObject *args) {
    return PyByteArray_FromStringAndSize((const char *) enabler.ccolor, 16*3*sizeof(float));
}
static PyObject*
dfr_window_size(PyObject *self, PyObject *args) {
    int w = 0, h = 0;
    SDL_GetWindowSize(dfr_self->window, &w, &h);  
    return Py_BuildValue("ii", w, h);
}

static PyObject*
dfr_toggle_fullscreen(PyObject *self, PyObject *args) {
    Uint32 wf = SDL_GetWindowFlags(dfr_self->window);
    if (wf & SDL_WINDOW_FULLSCREEN)
        SDL_SetWindowFullscreen(dfr_self->window, SDL_FALSE);
    else
        SDL_SetWindowFullscreen(dfr_self->window, SDL_TRUE);
    Py_RETURN_NONE;
}

static PyObject*
dfr_step_inc(PyObject *self, PyObject *args) {
    dfr_self->step += 1;
    Py_RETURN_NONE;
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
    {"ccolor", dfr_ccolor, METH_NOARGS,
     "Return enabler.ccolors as a bytearray." },
    {"window_size", dfr_window_size, METH_NOARGS,
     "Return this renderer's window size as a tuple." },
    {"toggle_fullscreen", dfr_toggle_fullscreen, METH_NOARGS,
     "as is says on the can" },    
     {"step_inc", dfr_step_inc, METH_NOARGS,
     "as is says on the can" },    
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

static void python_init(const std::string &libdir) {
    /* fiddle with env because gdb is compiled with py2.7 */
    
    std::string pylib;
    pylib += libdir;
    pylib += "/python3.2";
    std::string pypath;
    pypath += pylib;
    pypath += ":";
    pypath += pylib;
    pypath += "/site-packages:";
    pypath += pylib;
    pypath += "/plat-linux2:";
    pypath += pylib;
    pypath += "/lib-dynload";
    
    setenv("PYTHONPATH", pypath.c_str(),1);
    setenv("PGLIBDIR", libdir.c_str(),1);
    
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
    self->step = 0;
    _callcallback(self->py_render);
    self->step += 1;
    SDL_GL_SwapWindow(self->window);
    self->step += 1;
    self->frame += 1;
}

void renderer_glsl::zoom(zoom_commands cmd) {
    PyObject *pArgs = Py_BuildValue("i", cmd);
    PyObject *pRetVal = PyObject_CallObject(self->py_zoom, pArgs);
    Py_DECREF(pArgs);
    if (!pRetVal) {
        PyErr_Print();
        exit(1);
    }
    Py_DECREF(pRetVal);
}

void renderer_glsl::resize(int w, int h) {
    fprintf(stderr, "renderer_glsl::resize(%d, %d)\n", w, h);
    PyObject *pArgs = Py_BuildValue("ii", w,h);
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
    signal(SIGINT, SIG_DFL);

    char *dfp = getenv("DF_DIR");
    if (!dfp || (strlen(dfp) == 0)) {
        fprintf(stderr, "DF_DIR envvar is required.\n");
        exit(1);
    }
    self->libdir = dfp;
    self->libdir += "/libs";
    
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
    fprintf(stderr, "%s %s\nOpenGL %s GLSL %s\n", glGetString(GL_VENDOR), 
            glGetString(GL_RENDERER), glGetString(GL_VERSION), 
            glGetString(GL_SHADING_LANGUAGE_VERSION));
    int w,h;
    SDL_GetWindowSize(self->window, &w, &h);
    python_init(self->libdir);
    _callcallback(self->py_gl_init);
    //resize(w,h); // init grid and other shit. hmm. can't do before textures are here.
    
}
void renderer_glsl::accept_textures(textures &tm) {
    // screen and texpos referer to the index in the raws.
    // which is pos.
    // now just submit a list of SDL_Surface ptrs to the python.
    // std::vector<SDL_Surface *>& raws
    
    PyObject *plist = PyList_New(tm.raws.size()), *pval;
    
    for (int pos = 0; pos < tm.raws.size(); pos++) {
        SDL_Surface *surf = tm.raws[pos];
        if (surf) {
            if (NULL== (pval = Py_BuildValue("iIii", pos, surf, surf->w, surf->h)))
                PyErr_Print(), exit(1);
        } else {
            if (NULL== (pval = Py_BuildValue("iIii", pos, 0, 0, 0)))
                PyErr_Print(), exit(1);
        }
        if (-1 == PyList_SetItem(plist, pos, pval))
            PyErr_Print(), exit(1);
    }
    PyObject *pArgs = PyTuple_Pack(1, plist);
    PyObject *pRetVal = PyObject_CallObject(self->py_accept_textures, pArgs);
    Py_DECREF(pArgs);
    if (!pRetVal) {
        PyErr_Print();
        exit(1);
    }
    Py_DECREF(pRetVal);
}

bool renderer_glsl::get_mouse_coords(int &x, int &y) {
    if ( SDL_GetWindowFlags(self->window) && SDL_WINDOW_MOUSE_FOCUS) {
        SDL_GetMouseState(&x, &y);
        return true;
    }
    return false;
}

renderer_glsl::~renderer_glsl() { }
