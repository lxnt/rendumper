#include <cstdlib>
#include <map>

#include "iplatform.h"
#include "imqueue.h"
#include "isimuloop.h"

#include "SDL.h"
#include "glew.h"

#include "la_muerte_el_gl.h"

#define DFMODULE_BUILD
#include "irenderer.h"

namespace {

/*  Vertex array object streamer, derived from the fgt.gl.VAO0 and fgt.gl.SurfBunchPBO. */
struct vbstreamer_t {
    const unsigned rrlen;
    GLuint *va_names;
    GLuint *bo_names;
    GLsync *syncs;
    df_buffer_t **bufs;
    uint32_t w, h;
    uint32_t pot;   // preferred alignment of data as a power-of-two, in bytes
    int last_drawn; // hottest vao in our rr

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
        rrlen(rr)
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
}

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
    bufs = new buffer_t*[rrlen];
    glGenVertexArrays(rrlen, va_names);
    glGenBuffers(rrlen, bo_names);
    for (int i=0; i<rrlen; i++) {
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
        switch (glClientWaitSync(syncs[which], GL_SYNC_FLUSH_COMMANDS_BIT, 0) {
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
                platform->log_fatal("glClientWaitSync(syncs[%d]=%p): GL_WAIT_FAILED.", which, syncs[which]);
            default:
                platform->log_fatal("glClientWaitSync(syncs[%d]=%p): unbelievable.", which, syncs[which]);
        }
    }

    remap_buf(which);
    return bufs[which];
}

void vbstreamer_t::draw(df_buffer_t *buf) {
    int which;
    for (which = 0; which < rrlen ; which ++)
        if (bufs[which] == buf)
            break;

    if (bufs[which] != buf)
        platform->log_fatal("vbstreamer_t::draw(): bogus buffer supplied.");

    if (last_submitted == which)
        platform->log_fatal("vbstreamer_t::draw(): last_submitted == which :wtf.");

    glBindBuffer(GL_ARRAY_BUFFER, bo_names[which]);
    glUnmapBuffer(GL_ARRAY_BUFFER);

    GL_DEAD_YET();

    if ((w != buf->w) || (h != buf->h)) {
        /* discard frame: it's of wrong size anyway. reset vao while at it */
        /* this can cause spikes in fps: maybe don't submit those samples? */
        remap_buf(which);
        return;
    }
    glBindVertexArray(which);
    glDrawArrays(GL_POINTS, w*h);

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
void vbstreamer_t::remap_buf(int which) {
    bool reset_vao = ((w != bufs[which]->w) || (h != bufs[which]->h));

    if (reset_vao) {
        buf->ptr = NULL;
        buf->w = w, buf->h = h, buf->tail_sizeof = tail_sizeof;
        setup_buffer_t(buf, pot);
    }

    glBindBuffer(GL_ARRAY_BUFFER, bo_names[which]);
    buf->ptr = glMapBufferRange(GL_ARRAY_BUFFER, 0, bufs[mapped]->required_sz,
        GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT );

    setup_buffer_t(bufs[mapped], pot);

    /* generate grid positions */
    for (unsigned i = 0; i < w*h ; i++) {
        uint16_t *ptr = (uint32_t *)(buf->tail) + 4 * i;
        ptr[0] = i / w; // x | this column-major
        ptr[1] = i % h; // y |   major madness
        ptr[2] = 0;     // dim
        ptr[3] = 0;     // snow-rain
    }

    if (reset_vao) {
        glBindVertexArray(va_names[which]);
        glVertexAttribPointer(screen_posn,    4, GL_UNSIGNED_BYTE,  GL_FALSE, 0, DFBUFOFFS(bufs[which], screen));
        glVertexAttribPointer(texpos_posn,    1, GL_UNSIGNED_INT,   GL_FALSE, 0, DFBUFOFFS(bufs[which], texpos));
        glVertexAttribPointer(addcolor_posn,  1, GL_UNSIGNED_BYTE,  GL_FALSE, 0, DFBUFOFFS(bufs[which], addcolor));
        glVertexAttribPointer(grayscale_posn, 1, GL_UNSIGNED_BYTE,  GL_FALSE, 0, DFBUFOFFS(bufs[which], grayscale));
        glVertexAttribPointer(cf_posn,        1, GL_UNSIGNED_BYTE,  GL_FALSE, 0, DFBUFOFFS(bufs[which], cf));
        glVertexAttribPointer(cbr_posn,       1, GL_UNSIGNED_BYTE,  GL_FALSE, 0, DFBUFOFFS(bufs[which], cbr));
        glVertexAttribPointer(grid_posn,      4, GL_UNSIGNED_SHORT, GL_FALSE, 0, DFBUFOFFS(bufs[which], tail));
        glBindVertexArray(0);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    GL_DEAD_YET();
}

/* This will invalidate any storage in use. */
void vbstreamer_t::finalize() {
    for (int i = 0; i < rrlen ; i++) {
        GLint param;
        glBindBuffer(GL_ARRAY_BUFFER, bo_names[i]);
        glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_MAPPED, &param);
        if (param)
            glUnmapBuffer(GL_ARRAY_BUFFER);
        if (syncs[i])
            glDeleteSync(syncs[i]);
        delete bufs[i]
    }
    glDeleteVertexArrays(rrlen, va_names);
    glDeleteBuffers(rrlen, bo_names);
    delete bufs;
    delete bo_names;
    delete va_names;
    delete syncs;

    GL_DEAD_YET();
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

    void set_gridsize(unsigned w, unsigned h) { dimx = w, dimy = h; }
    uint32_t get_gridsize(void) { return (dimx << 16) | (dimy & 0xFFFF); }

    df_buffer_t *get_buffer();
    void submit_buffer(df_buffer_t *buf);

    void acknowledge(const itc_message_t&) {}

    void start();
    void join();
    void simuloop_quit() { not_done = false; }
    bool started;

    iplatform *platform;
    imqueue *mqueue;
    isimuloop *simuloop;

    imqd_t incoming_q;
    imqd_t ready_buffers_q

    vbstreamer_t streamer;

    implementation();

    unsigned dimx, dimy;
    unsigned dropped_frames;
    bool not_done;
    void renderer_thread();
    thread_t thread_id;

    SDL_Window *gl_window;
    SDL_GLContext gl_context;

    void slurp_keys();
    void initialize();
    void render_the_buffer(const df_buffer_t *buf);
    void do_the_flip();
    void post_free_buffer(df_buffer_t *);

    int Pszx, Pszy, Psz;                      // Point sprite size as drawn
    int viewport_offset_x, viewport_offset_y; // viewport tracking
    int viewport_w, viewport_h;               // for mouse coordinate transformation
};

void implementation::initialize() {

    /* this GL renderer needs or uses no SDL internal rendering
       or even a window surface. */
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, '0');

    /* hmm. platform does only defaults+timers? or video too?
       but PM:2D might want accelerated render driver.
       and InitSubsystem() calls VideoInit(NULL). Okay,
       let's just InitSubsystem() and see what happens. */
    if (SDL_InitSubsystem(SDL_INIT_VIDEO))
        platform->log_fatal("SDL_InitSubsystem(SDL_INIT_VIDEO): %s", SDL_GetError());

    struct gl_attr {
        SDL_GLattr attr; const char *name, int value;
    } attr_req[] = {
        (SDL_GL_RED_SIZE, "SDL_GL_RED_SIZE", 8),
        (SDL_GL_GREEN_SIZE, "SDL_GL_GREEN_SIZE", 8),
        (SDL_GL_BLUE_SIZE, "SDL_GL_BLUE_SIZE", 8),
        (SDL_GL_ALPHA_SIZE, "SDL_GL_ALPHA_SIZE", 8),
        (SDL_GL_DEPTH_SIZE, "SDL_GL_DEPTH_SIZE", 0),
        (SDL_GL_STENCIL_SIZE, "SDL_GL_STENCIL_SIZE", 0),
        (SDL_GL_DOUBLEBUFFER, "SDL_GL_DOUBLEBUFFER", 1),
        (SDL_GL_CONTEXT_MAJOR_VERSION, "SDL_GL_CONTEXT_MAJOR_VERSION", 2),
        (SDL_GL_CONTEXT_MINOR_VERSION, "SDL_GL_CONTEXT_MINOR_VERSION", 1)
        //(SDL_GL_CONTEXT_PROFILE_MASK, "SDL_GL_CONTEXT_PROFILE_MASK", cmask),
        //(SDL_GL_CONTEXT_FLAGS, "SDL_GL_CONTEXT_FLAGS", cflags)
    };

    for (int i=0; i < sizeof(attr_req)/sizeof(gl_attr); i++) {
        if (SDL_GL_SetAttribute(attr_req[i].attr, attr_req[i].value))
            platform->log_fatal("SDL_GL_SetAttribute(%s, %d): %s",
                attr_req[i].name, attr_req[i].value, SDL_GetError());
    }

    gl_window = SDL_CreateWindow("~some title~",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        pref_w, pref_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE); // there's some weird SDL_WINDOW_SHOWN flag

    if (!gl_window)
        platform->log_fatal("SDL_CreateWindow(...): %s", SDL_GetError());

    gl_context = SDL_GL_CreateContext(window);

    if (!gl_context)
        platform->log_fatal("SDL_GL_CreateContext(...): %s", SDL_GetError());

    for (int i=0; i < sizeof(attr_req)/sizeof(gl_attr); i++) {
        int value;
        if (SDL_GL_GetAttribute(attr_req[i].attr, &value))
            platform->log_fatal("SDL_GL_GetAttribute(%s, ptr): %s",
                attr_req[i].name, SDL_GetError());
        platform->log_info("GL context: %s requested %d got %d",
            attr_req[i].name, attr_req[i].value, value):
    }

    GLenum err = glewInit()
    if (GLEW_OK != err)
        platform->log_fatal("glewInit(): %s", glewGetErrorString(err));

    if (!glMapBufferRange)
        platform->log_fatal("ARB_map_buffer_range extension or OpenGL 3.0+ is required.");

    if (!glFenceSync)
        platform->log_fatal("ARB_sync extension or OpenGL 3.2+ is required.");

    if (!GLEW_ARB_map_buffer_alignment)
        platform->log_fatal("ARB_map_buffer_alignment extension or OpenGL 4.2+ is required.");

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

    GL_DEAD_YET();

    streamer.initialize();
}


void implementation::slurp_keys() {

}

void implementation::do_the_flip() { SDL_GL_SwapWindow(gl_window); }

void implementation::post_free_buffers() {
    /* to hell with timeouts. */
    while ((df_buffer_t *buf = streamer.get_a_buffer()) != NULL)
        mqueue->copy(free_buf_q, &buf, sizeof(df_buffer_t *), -1);
}

void implementation::renderer_thread(void) {
    while(not_done) {
        slurp_keys();


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
                        streamer->remap(buf);
                        post_free_buffer(buf);
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
            render_the_buffer(buf);
            free_buffer(buf);
            /* SDL_Flip() or whatever might look better in before render,
               so that previous frame gets rendered while input is being
               processed and other stuff is going on */
            do_the_flip();
        }
    }
}

/* Below go methods intended for other threads */

df_buffer_t *implementation::get_buffer(void) {
    df_buffer_t **bufp;
    size_t len;
    int rv;
    switch(rv = mqueue->recv(free_buf_q, bufp, &len, -1)) {
        case IMQ_OK:
            df_buffer_t *rv = *bufp;
            mqueue->free(buf);
            return rv;
        case IMQ_TIMEDOUT:
            return NULL;
        default:
            platform->log_fatal("%s: %d from mqueue->recv()", __func__, rv);
    }
}

int implementation get_mouse_state(int *x, int *y) {
    int mouse_x, mouse_y;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    x = (mouse_x - viewport_offset_x) / Pszx;
    y = (mouse_y - viewport_offset_y) / Pszy;
    return 42;
}

implementation::implementation() {
    started = false;
    not_done = true;
    dropped_frames = 0;
    platform = getplatform();
    simuloop = getsimuloop();
    mqueue = getmqueue();
    incoming_q = mqueue->open("renderer", 1<<10);
    free_buf_q = mqueue->open("free_buffers", 1<<10);

    initialize();
}

/* Below is code copied from renderer_ncurses.
   All curses-specific code and all comments were
   ripped out. All SDL/GL specific code is to be
   put into other methods or helper functions. */

/*  All those methods want to go into a parent class. */

void implementation::submit_buffer(df_buffer_t *buf) {
    itc_message_t msg;
    msg.t = itc_message_t::render_buffer;
    msg.d.buffer = buf;
    mqueue->copy(incoming_q, &msg, sizeof(itc_message_t), -1);
}

static int thread_stub(void *data) {
    implementation *owner = (implementation *) data;
    owner->renderer_thread();
    return 0;
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

void implementation::release(void) { }

static implementation *impl = NULL;
extern "C" DECLSPEC irenderer * APIENTRY getrenderer(void) {
    if (!impl)
        impl = new implementation();
    return impl;
}
} /* ns */