#include <cstdlib>
#include <map>
#include <ios>
#include <fstream>

#include "SDL.h"
#include "glew.h"

#include "iplatform.h"
#include "imqueue.h"
#include "isimuloop.h"
#include "itextures.h"
#include "la_muerte_el_gl.h"
#include "df_buffer.h"
#include "ansi_colors.h"

#define DFMODULE_BUILD
#include "irenderer.h"

namespace {

iplatform *platform;

const int MIN_GRID_X = 80;
const int MAX_GRID_X = 256;
const int MIN_GRID_Y = 25;
const int MAX_GRID_Y = 256;

#define MIN(a, b) ((a)<(b)?(a):(b))
#define MAX(a, b) ((a)>(b)?(a):(b))
#define CLAMP(a, a_floor, a_ceil) ( MIN( (a_floor), MAX( (a), (a_ceil) ) ) )
#define CLAMP_GW(w) CLAMP(w, MIN_GRID_X, MAX_GRID_X)
#define CLAMP_GH(h) CLAMP(h, MIN_GRID_Y, MAX_GRID_Y)

//{  vbstreamer_t
/*  Vertex array object streamer, derived from the fgt.gl.VAO0 and fgt.gl.SurfBunchPBO. */
struct vbstreamer_t {
    const unsigned rrlen;
    GLuint *va_names;
    GLuint *bo_names;
    GLsync *syncs;
    df_buffer_t **bufs;
    uint32_t w, h;
    uint32_t pot;   // preferred alignment of data as a power-of-two, in bytes
    unsigned last_drawn; // hottest vao in our rr

    const uint32_t tail_sizeof;

    /* attribute positions */
    const GLuint screen_posn;
    const GLuint texpos_posn;
    const GLuint addcolor_posn;
    const GLuint grayscale_posn;
    const GLuint cf_posn;
    const GLuint cbr_posn;
    const GLuint grid_posn;

    vbstreamer_t(unsigned rr = 3) :
        rrlen(rr),
        va_names(0),
        bo_names(0),
        syncs(0),
        bufs(0),
        w(0),
        h(0),
        pot(6),
        last_drawn(0),
        tail_sizeof(8), // 4*GLushort
        screen_posn(0),
        texpos_posn(1),
        addcolor_posn(2),
        grayscale_posn(3),
        cf_posn(4),
        cbr_posn(5),
        grid_posn(6) {}

    void initialize();
    df_buffer_t *get_a_buffer();
    void draw(df_buffer_t *);
    void finalize();
    void remap_buf(df_buffer_t *);
    unsigned find(const df_buffer_t *) const;
};

/* df_buffer_t::tail data is interleaved, unnormalized:
    - grid position in GL coordinate system, GLushort x2.
    - dim data from graphicst::dim_colors: GLushort dim
    - rain/snow: GLushort: 256*(bool rain) + bool snow
    for a stride of 8 bytes and passed as a single attr.
    grid position is generated here, at the same time
    dim, rain and snow get initialized to zero. */

void vbstreamer_t::initialize() {
    //glGetInteger(GL_MIN_MAP_BUFFER_ALIGNMENT, &pot);
    w = h = 0;
    va_names = new GLuint[rrlen];
    bo_names = new GLuint[rrlen];
    syncs = new GLsync[rrlen];
    memset(syncs, 0, sizeof(GLsync)*rrlen);
    bufs = new df_buffer_t*[rrlen];
    glGenVertexArrays(rrlen, va_names);
    glGenBuffers(rrlen, bo_names);
    for (unsigned i=0; i<rrlen; i++) {
        glBindVertexArray(va_names[i]);
        glBindBuffer(GL_ARRAY_BUFFER, bo_names[i]);
        glEnableVertexAttribArray(screen_posn);
        glEnableVertexAttribArray(texpos_posn);
        glEnableVertexAttribArray(addcolor_posn);
        glEnableVertexAttribArray(grayscale_posn);
        glEnableVertexAttribArray(cf_posn);
        glEnableVertexAttribArray(cbr_posn);
        glEnableVertexAttribArray(grid_posn);
        bufs[i] = new_buffer_t(0, 0, tail_sizeof);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    GL_DEAD_YET();
}

df_buffer_t *vbstreamer_t::get_a_buffer() {
    /* this defines the order of buffer submission */
    int which = last_drawn < rrlen - 1 ? last_drawn + 1 : 0;

    /* check if the buf is ready to be mapped */
    if (syncs[which]) {
        switch (glClientWaitSync(syncs[which], GL_SYNC_FLUSH_COMMANDS_BIT, 0)) {
            case GL_ALREADY_SIGNALED:
            case GL_CONDITION_SATISFIED:
                glDeleteSync(syncs[which]);
                syncs[which] = NULL;
                break;
            case GL_TIMEOUT_EXPIRED:
                /*  No buffers to map, GPU got stuffed.
                    We don't want to wait here, since that's going to
                    block simuloop() in before render_things().
                    It should take this fail as a hint to submit less frames. */
                return NULL;
            case GL_WAIT_FAILED:
                /* some fuckup with syncs[] */
                platform->fatal("glClientWaitSync(syncs[%d]=%p): GL_WAIT_FAILED.", which, syncs[which]);
            default:
                platform->fatal("glClientWaitSync(syncs[%d]=%p): unbelievable.", which, syncs[which]);
        }
    }

    remap_buf(bufs[which]);
    return bufs[which];
}

unsigned vbstreamer_t::find(const df_buffer_t *buf) const {
    unsigned which;
    for (which = 0; which < rrlen ; which ++)
        if (bufs[which] == buf)
            break;

    if (bufs[which] != buf)
        platform->fatal("vbstreamer_t::find(): bogus buffer supplied.");

    return which;
}

void vbstreamer_t::draw(df_buffer_t *buf) {
    unsigned which = find(buf);

    if (last_drawn == which)
        platform->fatal("vbstreamer_t::draw(): last_drawn == which :wtf.");

    glBindBuffer(GL_ARRAY_BUFFER, bo_names[which]);
    glUnmapBuffer(GL_ARRAY_BUFFER);

    GL_DEAD_YET();

    if ((w != buf->w) || (h != buf->h)) {
        /* discard frame: it's of wrong size anyway. reset vao while at it */
        /* this can cause spikes in fps: maybe don't submit those samples? */
        remap_buf(buf);
        platform->log_info("vbstreamer_t::draw(): remapped %d: grid mismatch", which);
        return;
    }
    platform->log_info("vbstreamer_t::draw(): drawing %d", which);

    glBindVertexArray(which);
    glDrawArrays(GL_POINTS, 0, w*h);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    syncs[which] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    last_drawn = which;

    GL_DEAD_YET();
}

/*  Hope the offsets don't change between buf maps.
    Otherwise we'll have to reset a VAO every frame.

    Expected state of *bufs[which]: unmapped, pointers invalid.
    Resulting state of *bufs[which]: mapped, pointers valid.
*/
void vbstreamer_t::remap_buf(df_buffer_t *buf) {
    bool reset_vao = ((w != buf->w) || (h != buf->h));

    if (reset_vao) {
        buf->ptr = NULL;
        buf->w = w, buf->h = h;
        buf->tail_sizeof = tail_sizeof;
        setup_buffer_t(buf, pot);
    }

    glBindBuffer(GL_ARRAY_BUFFER, bo_names[find(buf)]);
    buf->ptr = (uint8_t *)glMapBufferRange(GL_ARRAY_BUFFER, 0, buf->required_sz,
        GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT );

    setup_buffer_t(buf, pot);

    /* generate grid positions */
    for (unsigned i = 0; i < w*h ; i++) {
        uint16_t *ptr = (uint16_t *)(buf->tail) + 4 * i;
        ptr[0] = i / w; // x | this column-major
        ptr[1] = i % h; // y |   major madness
        ptr[2] = 0;     // dim
        ptr[3] = 0;     // snow-rain
    }

    if (reset_vao) {
        glBindVertexArray(va_names[find(buf)]);
        glVertexAttribPointer(screen_posn,    4, GL_UNSIGNED_BYTE,  GL_FALSE, 0, DFBUFOFFS(buf, screen));
        glVertexAttribPointer(texpos_posn,    1, GL_UNSIGNED_INT,   GL_FALSE, 0, DFBUFOFFS(buf, texpos));
        glVertexAttribPointer(addcolor_posn,  1, GL_UNSIGNED_BYTE,  GL_FALSE, 0, DFBUFOFFS(buf, addcolor));
        glVertexAttribPointer(grayscale_posn, 1, GL_UNSIGNED_BYTE,  GL_FALSE, 0, DFBUFOFFS(buf, grayscale));
        glVertexAttribPointer(cf_posn,        1, GL_UNSIGNED_BYTE,  GL_FALSE, 0, DFBUFOFFS(buf, cf));
        glVertexAttribPointer(cbr_posn,       1, GL_UNSIGNED_BYTE,  GL_FALSE, 0, DFBUFOFFS(buf, cbr));
        glVertexAttribPointer(grid_posn,      4, GL_UNSIGNED_SHORT, GL_FALSE, 0, DFBUFOFFS(buf, tail));
        glBindVertexArray(0);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    GL_DEAD_YET();
}

/* This will invalidate any storage in use. */
void vbstreamer_t::finalize() {
    for (unsigned i = 0; i < rrlen ; i++) {
        GLint param;
        glBindBuffer(GL_ARRAY_BUFFER, bo_names[i]);
        glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_MAPPED, &param);
        if (param)
            glUnmapBuffer(GL_ARRAY_BUFFER);
        if (syncs[i])
            glDeleteSync(syncs[i]);
        free_buffer_t(bufs[i]);
    }
    glDeleteVertexArrays(rrlen, va_names);
    glDeleteBuffers(rrlen, bo_names);
    delete bufs;
    delete bo_names;
    delete va_names;
    delete syncs;

    GL_DEAD_YET();
}
//} vbstreamer_t

//{  shader_t
/*  Shader container, derived from fgt.gl.GridShader. */
struct shader_t {
    GLuint program;

    const int tu_ansi, tu_font, tu_findex;
    int u_ansi_sampler;
    int u_font_sampler;
    int u_findex_sampler;
    int u_final_alpha;
    int u_pszar;
    int u_viewpoint;

    shader_t(): program(0), tu_ansi(0), tu_font(1), tu_findex(2) { }

    void initialize(const char *vsfname, const char *fsfname, const vbstreamer_t& );
    GLuint compile(const char *fname,  GLuint type);
    void use(float psz, float parx, float pary, int vp_x, int vp_y, float alpha);
    void finalize();

};

GLuint shader_t::compile(const char *fname, GLuint type) {
    GLuint target = glCreateShader(type);
    GLchar *buf;
    long len;

    std::ifstream f;
    f.open(fname, std::ios::binary);
    if (!f.is_open())
        platform->fatal("f.open(%s) failed.", fname);
    len = f.seekg(0, std::ios::end).tellg();
    f.seekg(0, std::ios::beg);
    buf = new GLchar[len + 1];
    f.read(buf, len);
    f.close();
    buf[len] = 0;

    glShaderSource(target, 1, const_cast<const GLchar**>(&buf), NULL);
    delete []buf;
    GL_DEAD_YET();
    glCompileShader(target);
    GL_DEAD_YET();

    return target;
}

void shader_t::initialize(const char *vsfname, const char *fsfname, const vbstreamer_t& vbs) {
    program = glCreateProgram();
    GLuint vs = compile(vsfname, GL_VERTEX_SHADER);
    GLuint fs = compile(fsfname, GL_FRAGMENT_SHADER);

    glAttachShader(program, vs);
    glDeleteShader(vs);
    glAttachShader(program, fs);
    glDeleteShader(fs);
    GL_DEAD_YET();

    glBindAttribLocation(program, vbs.screen_posn, "screen");
    glBindAttribLocation(program, vbs.texpos_posn, "texpos");
    glBindAttribLocation(program, vbs.addcolor_posn, "addcolor");
    glBindAttribLocation(program, vbs.grayscale_posn, "grayscale");
    glBindAttribLocation(program, vbs.cf_posn, "cf");
    glBindAttribLocation(program, vbs.cbr_posn, "cbr");
    glBindAttribLocation(program, vbs.grid_posn, "grid");

    glBindFragDataLocation(program, 0, "frag");

    GL_DEAD_YET();

    glLinkProgram(program);

    GLint stuff;
    GLchar strbuf[8192];
    glGetProgramiv(program, GL_LINK_STATUS, &stuff);
    if (stuff == GL_FALSE) {
        glGetProgramInfoLog(program, 8192, NULL, strbuf);
        platform->fatal("%s", strbuf);
    }

    u_ansi_sampler      = glGetUniformLocation(program, "ansi");
    u_font_sampler      = glGetUniformLocation(program, "font");
    u_findex_sampler    = glGetUniformLocation(program, "findex");
    u_final_alpha       = glGetUniformLocation(program, "final_alpha");
    u_pszar             = glGetUniformLocation(program, "pszar");
    u_viewpoint         = glGetUniformLocation(program, "viewpoint");

    GL_DEAD_YET();
}

void shader_t::use(float psz, float parx, float pary, int vp_x, int vp_y, float alpha) {
    glUseProgram(program);
    glUniform1i(u_ansi_sampler, tu_ansi);
    glUniform1i(u_font_sampler, tu_font);
    glUniform1i(u_findex_sampler, tu_findex);
    glUniform3f(u_pszar, psz, parx, pary);
    glUniform1f(u_final_alpha, alpha);
    glUniform2i(u_viewpoint, vp_x, vp_y);
}

void shader_t::finalize() {

}

//} shader_t

GLuint makeansitex(GLenum texture, ansi_colors_t ccolor) {
    GLuint texname;
    GLfloat ansi_stuff[16 * 4];
    for (int i = 0; i < 16; i++) {
        ansi_stuff[4 * i + 0] = ccolor[i][0] / 255.0;
        ansi_stuff[4 * i + 1] = ccolor[i][1] / 255.0;
        ansi_stuff[4 * i + 2] = ccolor[i][2] / 255.0;
        ansi_stuff[4 * i + 3] = 1.0;
    }
    glGenTextures(1, &texname);
    glActiveTexture(texture);
    glBindTexture(GL_TEXTURE_2D, texname);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_FLOAT, ansi_stuff);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    GL_DEAD_YET();
    return texname;
}

/*  OpenGL 2.1 renderer, rewrite of PRINT_MODE:SHADER. */

struct implementation : public irenderer {
    void release(void);

    void zoom_in();
    void zoom_out();
    void zoom_reset();
    void toggle_fullscreen();
    void override_grid_size(unsigned, unsigned);
    void release_grid_size();
    int mouse_state(int *mx, int *my);
    void reset_textures();

    void set_gridsize(unsigned w, unsigned h) { grid_w = w, grid_h = h; }
    uint32_t get_gridsize(void) { return (grid_w << 16) | (grid_h & 0xFFFF); }

    df_buffer_t *get_buffer();
    void submit_buffer(df_buffer_t *buf);

    void acknowledge(const itc_message_t&) {}

    void start();
    void join();
    void run_here();
    void simuloop_quit() { not_done = false; }
    bool started;

    imqueue *mqueue;
    isimuloop *simuloop;
    itextures *textures;

    imqd_t incoming_q;
    imqd_t free_buf_q;

    vbstreamer_t streamer;
    shader_t shader;

    df_texalbum_t *album;

    implementation();

    unsigned grid_w, grid_h;
    unsigned dropped_frames;
    bool not_done;
    void renderer_thread();
    thread_t thread_id;

    SDL_Window *gl_window;
    SDL_GLContext gl_context;

    void slurp_keys();
    void initialize();
    void finalize();
    void render_the_buffer(const df_buffer_t *buf);
    void reshape(int w, int h, int psz);

    float Parx, Pary;               // cel aspect ratio
    int Psz_native, Psz;            // larger dimension of cel 0
    int viewport_x, viewport_y;     // viewport tracking
    int viewport_w, viewport_h;     // for mouse coordinate transformation
    int mouse_xw, mouse_yw;         // mouse window coordinates
    int mouse_xg, mouse_yg;         // mouse grid coordinates
    GLuint ansitex;
    GLuint fonttex;
    GLuint findextex;
    bool cmd_zoom_in, cmd_zoom_out, cmd_zoom_reset, cmd_tex_reset;
};

void implementation::initialize() {

    /* this GL renderer needs or uses no SDL internal rendering
       or even a window surface. */
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "0");

    /* hmm. platform does only defaults+timers? or video too?
       but PM:2D might want accelerated render driver.
       and InitSubsystem() calls VideoInit(NULL). Okay,
       let's just InitSubsystem() and see what happens. */
    if (SDL_InitSubSystem(SDL_INIT_VIDEO))
        platform->fatal("SDL_InitSubsystem(SDL_INIT_VIDEO): %s", SDL_GetError());

    struct gl_attr {
        SDL_GLattr attr; const char *name; int value;
    } attr_req[] = {
        {SDL_GL_RED_SIZE, "SDL_GL_RED_SIZE", 8},
        {SDL_GL_GREEN_SIZE, "SDL_GL_GREEN_SIZE", 8},
        {SDL_GL_BLUE_SIZE, "SDL_GL_BLUE_SIZE", 8},
        {SDL_GL_ALPHA_SIZE, "SDL_GL_ALPHA_SIZE", 8},
        {SDL_GL_DEPTH_SIZE, "SDL_GL_DEPTH_SIZE", 0},
        {SDL_GL_STENCIL_SIZE, "SDL_GL_STENCIL_SIZE", 0},
        {SDL_GL_DOUBLEBUFFER, "SDL_GL_DOUBLEBUFFER", 1},
        {SDL_GL_CONTEXT_MAJOR_VERSION, "SDL_GL_CONTEXT_MAJOR_VERSION", 2},
        {SDL_GL_CONTEXT_MINOR_VERSION, "SDL_GL_CONTEXT_MINOR_VERSION", 1}
#if 0
        {SDL_GL_CONTEXT_PROFILE_MASK, "SDL_GL_CONTEXT_PROFILE_MASK", cmask},
        {SDL_GL_CONTEXT_FLAGS, "SDL_GL_CONTEXT_FLAGS", cflags}
#endif
    };

    for (unsigned i=0; i < sizeof(attr_req)/sizeof(gl_attr); i++) {
        if (SDL_GL_SetAttribute(attr_req[i].attr, attr_req[i].value))
            platform->fatal("SDL_GL_SetAttribute(%s, %d): %s",
                attr_req[i].name, attr_req[i].value, SDL_GetError());
    }

    gl_window = SDL_CreateWindow("~some title~",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        1280, 1024, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE); // there's some weird SDL_WINDOW_SHOWN flag

    if (!gl_window)
        platform->fatal("SDL_CreateWindow(...): %s", SDL_GetError());

    gl_context = SDL_GL_CreateContext(gl_window);

    if (!gl_context)
        platform->fatal("SDL_GL_CreateContext(...): %s", SDL_GetError());

    for (unsigned i=0; i < sizeof(attr_req)/sizeof(gl_attr); i++) {
        int value;
        if (SDL_GL_GetAttribute(attr_req[i].attr, &value))
            platform->fatal("SDL_GL_GetAttribute(%s, ptr): %s",
                attr_req[i].name, SDL_GetError());
        platform->log_info("GL context: %s requested %d got %d",
            attr_req[i].name, attr_req[i].value, value);
    }

    GLenum err = glewInit();
    if (GLEW_OK != err)
        platform->fatal("glewInit(): %s", glewGetErrorString(err));

    if (!glMapBufferRange)
        platform->fatal("ARB_map_buffer_range extension or OpenGL 3.0+ is required.");

    if (!glFenceSync)
        platform->fatal("ARB_sync extension or OpenGL 3.2+ is required.");

    if (!GLEW_ARB_map_buffer_alignment)
        platform->fatal("ARB_map_buffer_alignment extension or OpenGL 4.2+ is required.");

    /* todo:

    glGetInteger(GL_MAJOR_VERSION);
    glGetInteger(GL_MINOR_VERSION);
    glGetInteger(GL_CONTEXT_FLAGS); */

    /* set up whatever static state we need */
    glEnable(GL_POINT_SPRITE);
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    glGenTextures(1, &fonttex);
    glGenTextures(1, &findextex);

    GL_DEAD_YET();

    ansitex = makeansitex(GL_TEXTURE0, NULL);
    streamer.initialize();
    shader.initialize("lib/pms.vs", "lib/pms.fs", streamer);

    cmd_zoom_in = cmd_zoom_out = cmd_zoom_reset = cmd_tex_reset = false;
    album = NULL;
}

void implementation::finalize() {
    glDeleteTextures(1, &ansitex);
    glDeleteTextures(1, &fonttex);
    glDeleteTextures(1, &findextex);
    shader.finalize();
    streamer.finalize();
}

DFKeySym translate_sdl2_sym(SDL_Keycode sym) { return (DFKeySym) sym; }
uint16_t translate_sdl2_mod(uint16_t mod) { return mod; }

void implementation::slurp_keys() {
    SDL_Event sdl_event;
    df_input_event_t df_event;
    uint32_t now = platform->GetTickCount();

    while (SDL_PollEvent(&sdl_event)) {
        bool submit = true;

        df_event.now = now;
        df_event.mod = DFMOD_NONE;
        df_event.reports_release = true;
        df_event.sym = DFKS_UNKNOWN;
        df_event.unicode = 0;

        switch(sdl_event.type) {
        case SDL_TEXTINPUT:
            platform->log_info("SDL_TEXTINPUT: '%s'", sdl_event.text.text);
            submit = false;
            break;
        case SDL_KEYDOWN:
            df_event.type = df_input_event_t::DF_KEY_DOWN;
            df_event.sym = translate_sdl2_sym(sdl_event.key.keysym.sym);
            df_event.mod = translate_sdl2_mod(sdl_event.key.keysym.mod);
            df_event.unicode = sdl_event.key.keysym.unicode;
            break;
        case SDL_KEYUP:
            df_event.type = df_input_event_t::DF_KEY_UP;
            df_event.sym = translate_sdl2_sym(sdl_event.key.keysym.sym);
            df_event.mod = translate_sdl2_mod(sdl_event.key.keysym.mod);
            df_event.unicode = sdl_event.key.keysym.unicode;
            break;
        case SDL_MOUSEMOTION:
            submit = false;
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            switch (sdl_event.button.button) {
            case SDL_BUTTON_LEFT:
                df_event.button = df_input_event_t::DF_BUTTON_LEFT;
                break;
            case SDL_BUTTON_MIDDLE:
                df_event.button = df_input_event_t::DF_BUTTON_RIGHT;
                break;
            case SDL_BUTTON_RIGHT:
                df_event.button = df_input_event_t::DF_BUTTON_MIDDLE;
                break;
            case SDL_BUTTON_X1:
                platform->log_info("SDL_BUTTON_X1: '%s'", sdl_event.text.text);
                submit = false;
                break;
            case SDL_BUTTON_X2:
                platform->log_info("SDL_BUTTON_X2: '%s'", sdl_event.text.text);
                submit = false;
                break;
            default:
                submit = false;
                break;
            }
            if (sdl_event.type == SDL_MOUSEBUTTONDOWN)
                df_event.type = df_input_event_t::DF_BUTTON_DOWN;
            else
                df_event.type = df_input_event_t::DF_BUTTON_UP;
            break;
        case SDL_MOUSEWHEEL:
            platform->log_info("SDL_MOUSEWHEEL: x=%d y=%d", sdl_event.wheel.x, sdl_event.wheel.y);
            if (sdl_event.wheel.y < 0) {
                df_event.type = df_input_event_t::DF_BUTTON_DOWN;
                df_event.button = df_input_event_t::DF_WHEEL_UP;
            } else if (sdl_event.wheel.y > 0) {
                df_event.type = df_input_event_t::DF_BUTTON_DOWN;
                df_event.button = df_input_event_t::DF_WHEEL_DOWN;
            } else if (sdl_event.wheel.y == 0) {
                submit = false;
            }
            break;
        case SDL_WINDOWEVENT:
            switch (sdl_event.window.event) {
            case SDL_WINDOWEVENT_FOCUS_GAINED:
            case SDL_WINDOWEVENT_SHOWN:
            case SDL_WINDOWEVENT_EXPOSED:
                break;
            case SDL_WINDOWEVENT_RESIZED:
                reshape(sdl_event.window.data1, sdl_event.window.data2, -1);
            default:
                break;
            }
            submit = false;
            break;
        case SDL_WINDOWEVENT_CLOSE:
            platform->log_info("SDL_WINDOWEVENT_CLOSE");
            submit = false;
            break;
        case SDL_QUIT:
            df_event.type = df_input_event_t::DF_QUIT;
            break;

        default:
            submit = false;
            break;
        }
        if (submit)
            simuloop->add_input_event(&df_event);
    }

    { // update mouse state
        SDL_GetMouseState(&mouse_xw, &mouse_yw);
        mouse_xg = (mouse_xw - viewport_x) / (Parx * Psz);
        mouse_yg = (mouse_yw - viewport_y) / (Pary * Psz);
    }

}

void implementation::renderer_thread(void) {
    while(not_done) {

        slurp_keys();

        if (cmd_zoom_in) {
            cmd_zoom_in = cmd_zoom_out = cmd_zoom_reset = false;
            reshape(-1, -1, Psz + 1);
        }
        if (cmd_zoom_out) {
            cmd_zoom_in = cmd_zoom_out = cmd_zoom_reset = false;
            if (Psz > 3) // don't be ridiculous
                reshape(-1, -1, Psz - 1);
        }
        if (cmd_zoom_reset) {
            cmd_zoom_in = cmd_zoom_out = cmd_zoom_reset = false;
            reshape(-1, -1, Psz_native);
        }
        if (cmd_tex_reset) {
            cmd_tex_reset = false;
            if (album)
                textures->release_album(album);
            album = textures->get_album();
            int cw = album->index[0].rect.w;
            int ch = album->index[0].rect.h;
            if (cw > ch) {
                Parx = 1.0;
                Pary = (float)ch/(float)cw;
                Psz = Psz_native = cw;
            } else {
                Parx = (float)cw/(float)ch;
                Pary = 1.0;
                Psz = Psz_native = ch;
            }
            reshape(-1, -1, Psz);
        }

        df_buffer_t *buf = NULL;
        unsigned read_timeout_ms = 10;
        while (true) {
            void *vbuf; size_t vlen;
            int rv = mqueue->recv(incoming_q, &vbuf, &vlen, read_timeout_ms);
            read_timeout_ms = 0;

            if (rv == IMQ_TIMEDOUT)
                break;

            if (rv != 0)
                platform->fatal("render_thread(): %d from mqueue->recv().");

            itc_message_t *msg = (itc_message_t *)vbuf;

            switch (msg->t) {
                case itc_message_t::render_buffer:
                    if (buf) {
                        dropped_frames ++;
                        streamer.remap_buf(buf);
                    }
                    buf = msg->d.buffer;
                    mqueue->free(msg);
                    break;
                default:
                    platform->log_error("render_thread(): unknown message type %d", msg->t);
                    break;
            }
        }
        if (buf) {
        }
    }
}

/*

resize: just make grid fit into the window; adjust viewport to center it.
zoom: same, only psz changes, not window size.

all based on proportions of the base celpage, rest of cels get shrunk or stretched.

see fgt.gl.rednerer.reshape()

*/
void implementation::reshape(int new_window_w, int new_window_h, int new_psz) {
    platform->log_info("reshape(): got window %dx%d psz %d",new_window_w, new_window_h, new_psz);

    if (!album || !album->count) { // can't draw anything without textures anyway
        platform->log_info("reshape(): no textures.");
        return;
    }

    if ( (new_window_w > 0) && (new_window_h > 0) ) { // a resize
        if (new_psz > 0)
            platform->fatal("reshape + zoom : can't");
    } else { // a zoom
        SDL_GetWindowSize(gl_window, &new_window_w, &new_window_h);
        Psz = new_psz;
    }

    int new_grid_w = new_window_w / (Psz * Parx);
    int new_grid_h = new_window_h / (Psz * Pary);

    grid_w = MIN(MAX(new_grid_w, MIN_GRID_X), MAX_GRID_X);
    grid_h = MIN(MAX(new_grid_h, MIN_GRID_Y), MAX_GRID_Y);

    viewport_w = grid_w * Psz * Parx;
    viewport_h = grid_h * Psz * Pary;
    viewport_x = ( new_window_w - viewport_w ) / 2;
    viewport_y = ( new_window_h - viewport_h ) / 2;

    glViewport(viewport_x, viewport_y, viewport_w, viewport_h);
}

/* Below go methods intended for other threads */

/* return value of NULL means: skip that render_things() call. */
df_buffer_t *implementation::get_buffer(void) {
    void *msg = NULL;
    df_buffer_t *buf = NULL;
    size_t len = sizeof(void *);
    int rv;
    switch (rv = mqueue->recv(free_buf_q, &msg, &len, 0)) {
        case IMQ_OK:
            buf = (df_buffer_t *)msg;
            mqueue->free(msg);
            return buf;
        case IMQ_TIMEDOUT:
            break;
        default:
            platform->fatal("%s: %d from mqueue->recv()", __func__, rv);
    }
    /* whoops, free_buf_q is empty. try to fill it */

    if ((buf = streamer.get_a_buffer()) != NULL) {
        /* whoa, streamer got buffers: rid it of them */
        df_buffer_t *tbuf;
        while ((tbuf = streamer.get_a_buffer()) != NULL)
            if ((rv = mqueue->copy(free_buf_q, (void *)&tbuf, sizeof(void *), -1)))
                platform->fatal("%s: %d from mqueue->copy()", __func__, rv);
    }

    return buf;
}


void implementation::zoom_in() { cmd_zoom_in = true; }
void implementation::zoom_out() { cmd_zoom_out = true; }
void implementation::zoom_reset() { cmd_zoom_reset = true;  }
void implementation::toggle_fullscreen() { platform->log_info("toggle_fullscreen(): stub"); }
void implementation::override_grid_size(unsigned, unsigned)  { platform->log_info("override_grid_size(): stub"); }
void implementation::release_grid_size() { platform->log_info("release_grid_size(): stub"); }
void implementation::reset_textures() { cmd_tex_reset = true;  }
int implementation::mouse_state(int *x, int *y) {
    /* race-prone. convert to uint32_t mouse_gxy if it causes problems. */
    *x = mouse_xg;
    *y = mouse_yg;
    return 42;
}

implementation::implementation() {
    started = false;
    not_done = true;
    dropped_frames = 0;
    textures = gettextures();
    simuloop = getsimuloop();
    mqueue = getmqueue();
    if ((incoming_q = mqueue->open("renderer", 1<<10)) < 0)
        platform->fatal("%s: %d from mqueue->open(renderer)", __func__, incoming_q);

    if ((free_buf_q = mqueue->open("free_buffers", 1<<10)) < 0)
        platform->fatal("%s: %d from mqueue->open(free_buffers)", __func__, free_buf_q);
}

/* Below is code copied from renderer_ncurses.
   All curses-specific code and all comments were
   ripped out. All SDL/GL specific code is to be
   put into other methods or helper functions. */

//{  All those methods want to go into a parent class.

void implementation::submit_buffer(df_buffer_t *buf) {
    itc_message_t msg;
    msg.t = itc_message_t::render_buffer;
    msg.d.buffer = buf;
    mqueue->copy(incoming_q, &msg, sizeof(itc_message_t), -1);
}

static int thread_stub(void *data) {
    implementation *owner = (implementation *) data;
    owner->initialize();
    owner->renderer_thread();
    return 0;
}

void implementation::run_here() {
    if (started) {
        platform->log_error("second renderer start ignored\n");
        return;
    }
    started = true;
    initialize();
    renderer_thread();
}

void implementation::start() {
    if (started) {
        platform->log_error("second renderer start ignored\n");
        return;
    }
    started = true;
    thread_id = platform->thread_create(thread_stub, "renderer", (void *)this);
}

void implementation::join() {
    if (!started) {
        platform->log_error("irenderer::join(): not started\n");
        return;
    }
    platform->thread_join(thread_id, NULL);
}

//}

void implementation::release(void) { }

static implementation *impl = NULL;
extern "C" DECLSPEC irenderer * APIENTRY getrenderer(void) {
    if (!impl) {
        platform = getplatform();
        impl = new implementation();
    }
    return impl;
}
} /* ns */
