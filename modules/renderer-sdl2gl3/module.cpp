#include <cstdlib>
#include <vector>
#include <ios>
#include <fstream>

#include "glew.h"

#include "SDL.h"
#include "SDL_pnglite.h"

#include "iplatform.h"
#include "imqueue.h"
#include "isimuloop.h"
#include "itextures.h"
#include "la_muerte_el_gl.h"
#include "df_buffer.h"
#include "utf8decode.h"
#include "df_text.h"

#define DFMODULE_BUILD
#include "irenderer.h"

iplatform *platform = NULL;
getplatform_t _getplatform = NULL;

char *get_embedded_shader_source(const char *setname, const unsigned int type);

namespace {

imqueue   *mqueue   = NULL;
isimuloop *simuloop = NULL;
itextures *textures = NULL;

const int MIN_GRID_X = 80;
const int MAX_GRID_X = 256;
const int MIN_GRID_Y = 25;
const int MAX_GRID_Y = 256;

/* increasing MAX_GRID_X*MAX_GRID_Y to beyond 64K
   will require changing format of the vtx id attribute.
   Is not to be done for GL[ES] 2.0 */

#define MIN(a, b) ((a)<(b)?(a):(b))
#define MAX(a, b) ((a)>(b)?(a):(b))
#define CLAMP(a, a_floor, a_ceil) ( MIN( (a_floor), MAX( (a), (a_ceil) ) ) )
#define CLAMP_GW(w) CLAMP(w, MIN_GRID_X, MAX_GRID_X)
#define CLAMP_GH(h) CLAMP(h, MIN_GRID_Y, MAX_GRID_Y)

/* buffer states:
    - unmapped, not in any use - initial state.
    - mapped, not in any use
    - mapped, in free_buffers queue
    - mapped, given out to simuthread
    - mapped, submitted to renderer - in inbound_q
    - unmapped, fence object not signalled - being drawn
    - unmapped, fence object signalled - drawing finished, can be remapped.

    in buf::pstate :


*/
enum bufstate_t : uint32_t {
    BS_NONE = 0,            // unmapped, just allocated                  - ours
    BS_MAPPED_UNUSED,       // mapped, ready to be given out             - ours
    BS_FREE_Q,              // mapped, in renderer's free_buf_q          - not ours
    BS_IN_SIMULOOP,         // mapped, handed out to the simuloop        - not ours
    BS_INBOUND_Q,           // mapped, handed back via submit_buffer, sits in incoming_q - not ours. submitted to draw() or remap(), where it becomes ours.
    BS_RENDERING,           // unmapped, fence object might be pending   - ours.
    BS_RENDER_DONE          // unmapped, fence object was signalled      - ours.
};

const char *buf_pstate_str[] = {
    "BS_NONE",
    "BS_MAPPED_UNUSED",
    "BS_FREE_Q",
    "BS_IN_SIMULOOP",
    "BS_INBOUND_Q",
    "BS_RENDERING",
    "BS_RENDER_DONE",
    NULL
};


//{  vbstreamer_t
/*  Vertex array object streamer, derived from the fgt.gl.VAO0 and fgt.gl.SurfBunchPBO. */
struct vbstreamer_t {
    ilogger *logr;

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
    const GLuint fx_posn;

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
        tail_sizeof(0), // not used
        screen_posn(0),
        texpos_posn(1),
        addcolor_posn(2),
        grayscale_posn(3),
        cf_posn(4),
        cbr_posn(5),
        fx_posn(6) {}

    void initialize();
    df_buffer_t *get_a_buffer();
    void draw(df_buffer_t *);
    void finalize();
    void remap_buf(df_buffer_t *);
    void set_grid(uint32_t, uint32_t);
    unsigned find(const df_buffer_t *) const;
};

/* df_buffer_t::tail data is now used only for emulating
    gl_VertexID for GL2. It would be better to move
    this to a separate GL_STATIC_DRAW BO.
    Waiting on vbstreamer_t interface refactoring to allow
    multiple BOs per one vbstreamer. */

void vbstreamer_t::initialize() {
    logr = platform->getlogr("gl.vbstreamer");
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
        glEnableVertexAttribArray(fx_posn);
#if !defined(MALLOC_GLBUFFERS)
        bufs[i] = new_buffer_t(0, 0, tail_sizeof, pot);
#else
        bufs[i] = allocate_buffer_t(0, 0, tail_sizeof, pot);
#endif
    }
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    GL_DEAD_YET();
}

df_buffer_t *vbstreamer_t::get_a_buffer() {
    GLenum srv;
    int free_q = 0;

    for (unsigned which = 0; which < rrlen ; which ++) {
        df_buffer_t *buf = bufs[which];
        switch (buf->pstate) {
            case BS_NONE:
                remap_buf(buf);
            case BS_MAPPED_UNUSED:
                return buf;
            case BS_RENDERING:
                if (!syncs[which])
                    logr->fatal("buf %p/%d BS_RENDERING w/o sync object", buf, which);
                switch (srv = glClientWaitSync(syncs[which], GL_SYNC_FLUSH_COMMANDS_BIT, 0)) {
                    case GL_ALREADY_SIGNALED:
                    case GL_CONDITION_SATISFIED:
                        glDeleteSync(syncs[which]);
                        syncs[which] = NULL;
                        logr->trace("get_a_buffer(): sync for %p/%d signalled/satisfied", buf, which);
                        buf->pstate = BS_RENDER_DONE;
                        remap_buf(buf);
                        return buf;
                    case GL_TIMEOUT_EXPIRED:
                        logr->trace("get_a_buffer(): sync for %p/%d not signalled yet", buf, which);
                        break;
                    case GL_WAIT_FAILED:
                        /* some fuckup with syncs[] */
                        logr->fatal("glClientWaitSync(syncs[%d]=%p): GL_WAIT_FAILED.", which, syncs[which]);
                    default:
                        GL_DEAD_YET();
                        logr->fatal("glClientWaitSync(syncs[%d]=%p): %x : unbelievable.", which, syncs[which], srv);
                }
            case BS_FREE_Q:
                free_q ++;
                break;
            default:
                logr->trace("get_a_buffer(): skipping %p/%d: pstate=%s", buf, which,
                                        buf_pstate_str[buf->pstate]);
        }
    }
    if (!free_q)
        logr->warn("get_a_buffer(): no buffers and free_q==0.");
    return NULL;
}

void vbstreamer_t::set_grid(uint32_t _w, uint32_t _h) {
    if ((w != _w) || (h != _h))
        logr->info("set_grid(): %ux%u -> %ux%u",w, h, _w, _h);
    w = _w, h = _h;
}

unsigned vbstreamer_t::find(const df_buffer_t *buf) const {
    unsigned which;
    for (which = 0; which < rrlen ; which ++)
        if (bufs[which] == buf)
            break;

    if (bufs[which] != buf)
        logr->fatal("find(): bogus buffer supplied.");

    return which;
}

void vbstreamer_t::draw(df_buffer_t *buf) {
    unsigned which = find(buf);

    if (buf->pstate != BS_INBOUND_Q)
        logr->fatal("draw(%p/%d): pstate == %u :wtf.", buf, which, buf->pstate);

    glBindBuffer(GL_ARRAY_BUFFER, bo_names[which]);

    if (false && logr->enabled(LL_TRACE)) {
        char name[4096];
        sprintf(name, "tail-%d.dump", which);
        dump_buffer_t(buf, name);
    }

    if ((w != buf->w) || (h != buf->h)) {
        /* discard frame: it's of wrong size anyway. reset vao while at it */
        /* this can cause spikes in fps: maybe don't submit those samples? */
        remap_buf(buf);
        logr->info("draw(%p/%d): remapped: grid mismatch", buf, which);
        return;
    }
#if !defined(MALLOC_GLBUFFERS)
    glUnmapBuffer(GL_ARRAY_BUFFER);
#else
    glBufferData(GL_ARRAY_BUFFER, buf->used_sz, buf->screen, GL_STREAM_DRAW);
#endif
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    GL_DEAD_YET();

    logr->trace("draw(%p/%d): drawing %d points", buf, which, w*h);

    glBindVertexArray(va_names[which]);
    glDrawArrays(GL_POINTS, 0, w*h);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    syncs[which] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    last_drawn = which;
    buf->pstate = BS_RENDERING;

    GL_DEAD_YET();
}

/*  Hope the offsets don't change between buf maps.
    Otherwise we'll have to reset a VAO every frame.

    Expected state of *bufs[which]: unmapped, pointers invalid.
    Resulting state of *bufs[which]: mapped, pointers valid.
*/
void vbstreamer_t::remap_buf(df_buffer_t *buf) {
    bool reset_vao = ((w != buf->w) || (h != buf->h));
    unsigned which = find(buf);
    logr->trace("remap_buf(%p/%d): state=%s reset_vao=%s vbs.wh=%dx%d buf->wh=%dx%d",
        buf, which, buf_pstate_str[buf->pstate], reset_vao ? "true": "false", w, h, buf->w, buf->h);
    glBindBuffer(GL_ARRAY_BUFFER, bo_names[which]);

    if (buf->pstate == BS_INBOUND_Q) {
        /* a skipped frame */
        if (!reset_vao) {
            /* can be recycled w/o remapping (what about dim/snow?) */
            buf->pstate = BS_MAPPED_UNUSED;
            logr->trace("remap_buf(%p/%d): recycled fast", buf, which);
            return;
        }
#if !defined(MALLOC_GLBUFFERS)
        glUnmapBuffer(GL_ARRAY_BUFFER);
#endif
        buf->pstate = BS_RENDER_DONE;
        GL_DEAD_YET();
    }

    if ((buf->pstate != BS_RENDER_DONE) && (buf->pstate != BS_NONE))
        logr->fatal("remap_buf(%p/%d): state=%s after fixup", buf, which, buf_pstate_str[buf->pstate]);

    if (reset_vao) {
#if !defined(MALLOC_GLBUFFERS)
        buf->ptr = NULL;
        buf->w = w, buf->h = h;
        buf->tail_sizeof = tail_sizeof;
        setup_buffer_t(buf, pot); // get required_sz
        glBufferData(GL_ARRAY_BUFFER, buf->required_sz, NULL, GL_DYNAMIC_DRAW);
#else
        realloc_buffer_t(buf, w, h, tail_sizeof);
#endif
    }

#if !defined(MALLOC_GLBUFFERS)
    buf->ptr = (uint8_t *)glMapBufferRange(GL_ARRAY_BUFFER, 0, buf->required_sz,
//        GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT ); <- invalid flags in Mesa. Why?
//        GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT );
        GL_MAP_READ_BIT | GL_MAP_WRITE_BIT );
#endif

    GL_DEAD_YET();

    setup_buffer_t(buf);

    if (reset_vao) {
        glBindVertexArray(va_names[find(buf)]);
        glVertexAttribIPointer(screen_posn,    4, GL_UNSIGNED_BYTE,  0, DFBUFOFFS(buf, screen));
        glVertexAttribIPointer(texpos_posn,    1, GL_UNSIGNED_INT,   0, DFBUFOFFS(buf, texpos));
        glVertexAttribIPointer(addcolor_posn,  1, GL_UNSIGNED_BYTE,  0, DFBUFOFFS(buf, addcolor));
        glVertexAttribIPointer(grayscale_posn, 1, GL_UNSIGNED_BYTE,  0, DFBUFOFFS(buf, grayscale));
        glVertexAttribIPointer(cf_posn,        1, GL_UNSIGNED_BYTE,  0, DFBUFOFFS(buf, cf));
        glVertexAttribIPointer(cbr_posn,       1, GL_UNSIGNED_BYTE,  0, DFBUFOFFS(buf, cbr));
        glVertexAttribIPointer(fx_posn,        1, GL_UNSIGNED_BYTE,  0, DFBUFOFFS(buf, fx));
        glBindVertexArray(0);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    buf->pstate = BS_MAPPED_UNUSED;

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
    delete[] bufs;
    delete[] bo_names;
    delete[] va_names;
    delete[] syncs;
    GL_DEAD_YET();
}
//} vbstreamer_t

//{  shader_t
/*  Shader container, derived from fgt.gl.Shader0. */

struct shader_t {
    ilogger *logr;
    GLuint program;

    shader_t(): program(0) { }

    virtual void bind_attribute_locs() = 0;
    virtual void get_uniform_locs() = 0;

    void initialize(const char *setname);
    GLuint compile(const char *fname,  GLuint type);
    void finalize();

};

GLuint shader_t::compile(const char *setname, GLuint type) {
    GLuint target = glCreateShader(type);
    char *buf = NULL;
    long len;

    std::string fname(DFM_SHADERS_PATH);
    std::string stype;
    if (fname[fname.size()-1] != '/')
        fname += '/';
    fname += setname;
    switch (type) {
    case GL_VERTEX_SHADER:
        fname += ".vs";
        stype = "vertex";
        break;
    case GL_FRAGMENT_SHADER:
        fname += ".fs";
        stype = "fragment";
        break;
    default:
        logr->fatal("compile(%s, %d): ~unk~ type");
    }

    buf = get_embedded_shader_source(setname, type);

    std::ifstream f;
    bool delete_buf = false;
    f.open(fname, std::ios::binary);

    if (f.is_open()) {
        len = f.seekg(0, std::ios::end).tellg();
        f.seekg(0, std::ios::beg);
        buf = new GLchar[len + 1];
        f.read(buf, len);
        f.close();
        buf[len] = 0;
        delete_buf = true;
    } else {
        logr->trace("open(%s) failed; ignoring.", fname.c_str());
    }

    if (buf == NULL)
        logr->fatal("compile(): shader set '%s' not found.", setname);

    glShaderSource(target, 1, const_cast<const GLchar**>(&buf), NULL);

    if (delete_buf)
        delete []buf;

    bool using_embedded = not delete_buf;

    GL_DEAD_YET();
    glCompileShader(target);

    GLint param;
    glGetShaderiv(target, GL_COMPILE_STATUS, &param);
    if (param != GL_TRUE) {
        glGetShaderiv(target, GL_INFO_LOG_LENGTH, &param);
        buf = new GLchar[param + 1];
        glGetShaderInfoLog(target, param, NULL, buf);
        if (using_embedded)
            logr->error("embedded shader %s/%s compile: %s", setname, stype.c_str(), buf);
        else
            logr->error("'%s' compile: %s", fname.c_str(), buf);
        delete []buf;
    } else {
        if (using_embedded)
            logr->info("%s/%s compiled ok.", setname, stype.c_str());
        else
            logr->info("'%s' compiled ok.", fname.c_str());
    }

    GL_DEAD_YET();

    return target;
}

void shader_t::initialize(const char *setname) {
    logr = platform->getlogr("gl.shader");

    program = glCreateProgram();
    GLuint vs = compile(setname, GL_VERTEX_SHADER);
    GLuint fs = compile(setname, GL_FRAGMENT_SHADER);

    glAttachShader(program, vs);
    glDeleteShader(vs);
    glAttachShader(program, fs);
    glDeleteShader(fs);
    GL_DEAD_YET();

    bind_attribute_locs();

    glLinkProgram(program);
    GL_DEAD_YET();

    GLint stuff;
    GLchar strbuf[8192];
    glGetProgramiv(program, GL_LINK_STATUS, &stuff);
    if (stuff == GL_FALSE) {
        glGetProgramInfoLog(program, 8192, NULL, strbuf);
        logr->fatal("%s", strbuf);
    }
    GL_DEAD_YET();

    get_uniform_locs();
}

void shader_t::finalize() {
    glDeleteProgram(program);
    GL_DEAD_YET();
}

//}
//{ grid_shader_t
/*  Grid shader, derived from fgt.gl.GridShader. */

struct grid_shader_t : public shader_t {
    /* attribute positions - copy from vbstreamer_t */
    const GLuint screen_posn;
    const GLuint texpos_posn;
    const GLuint addcolor_posn;
    const GLuint grayscale_posn;
    const GLuint cf_posn;
    const GLuint cbr_posn;
    const GLuint fx_posn;

    /* uniform locations */
    int u_font_sampler;
    int u_findex_sampler;
    int u_final_alpha;
    int u_pszar;
    int u_grid_wh;
    int u_colors;

    grid_shader_t(): shader_t(),
        screen_posn(0),
        texpos_posn(1),
        addcolor_posn(2),
        grayscale_posn(3),
        cf_posn(4),
        cbr_posn(5),
        fx_posn(6) {}

    void bind_attribute_locs();
    void get_uniform_locs();

    void set_at_frame(float);
    void set_at_resize(float, float, int, unsigned, unsigned);
    void set_at_font(int, int, const ansi_colors_t *);
};

void grid_shader_t::bind_attribute_locs() {

    glBindAttribLocation(program, screen_posn, "screen");
    glBindAttribLocation(program, texpos_posn, "texpos");
    glBindAttribLocation(program, addcolor_posn, "addcolor");
    glBindAttribLocation(program, grayscale_posn, "grayscale");
    glBindAttribLocation(program, cf_posn, "cf");
    glBindAttribLocation(program, cbr_posn, "cbr");
    glBindAttribLocation(program, fx_posn, "fx");

    //glBindFragDataLocation(program, 0, "frag");

    GL_DEAD_YET();
    logr->trace("%s: %d", "va screen_posn", screen_posn);
    logr->trace("%s: %d", "va texpos_posn", texpos_posn);
    logr->trace("%s: %d", "va addcolor_posn", addcolor_posn);
    logr->trace("%s: %d", "va grayscale_posn", grayscale_posn);
    logr->trace("%s: %d", "va cf_posn", cf_posn);
    logr->trace("%s: %d", "va cbr_posn", cbr_posn);
    logr->trace("%s: %d", "va fx_posn", fx_posn);

}

void grid_shader_t::get_uniform_locs() {

    u_font_sampler      = glGetUniformLocation(program, "font");
    u_findex_sampler    = glGetUniformLocation(program, "findex");
    u_final_alpha       = glGetUniformLocation(program, "final_alpha");
    u_pszar             = glGetUniformLocation(program, "pszar");
    u_grid_wh           = glGetUniformLocation(program, "grid_wh");
    u_colors            = glGetUniformLocation(program, "colors");
    if (u_colors == -1)
        u_colors        = glGetUniformLocation(program, "colors[0]");

    GL_DEAD_YET();

    logr->trace("%s: %d", "uf font_sampler", u_font_sampler);
    logr->trace("%s: %d", "uf findex_sampler", u_findex_sampler);
    logr->trace("%s: %d", "uf final_alpha", u_final_alpha);
    logr->trace("%s: %d", "uf pszar", u_pszar);
    logr->trace("%s: %d", "uf colors", u_colors);
}

void grid_shader_t::set_at_frame(float alpha) {
    glUseProgram(program);
    glUniform1f(u_final_alpha, alpha); GL_DEAD_YET();
}

void grid_shader_t::set_at_resize(float parx, float pary, int psz,
                                unsigned grid_w, unsigned grid_h) {
    glUseProgram(program);
    glUniform3f(u_pszar, parx, pary, psz);      GL_DEAD_YET();
    glUniform2i(u_grid_wh, grid_w, grid_h);     GL_DEAD_YET();
}

void grid_shader_t::set_at_font(int tu_font, int tu_findex,
                                    const ansi_colors_t *ccolors) {

    GLfloat fcolors[16*4];
    for (int i = 0; i <16 ; i++) {
        fcolors[4*i + 0] = (float) ((*ccolors)[i][0]) / 255.0;
        fcolors[4*i + 1] = (float) ((*ccolors)[i][1]) / 255.0;
        fcolors[4*i + 2] = (float) ((*ccolors)[i][2]) / 255.0;
        fcolors[4*i + 3] = 1.0f;
    }

    glUseProgram(program);
    glUniform1i(u_font_sampler, tu_font);       GL_DEAD_YET();
    glUniform1i(u_findex_sampler, tu_findex);   GL_DEAD_YET();
    glUniform4fv(u_colors, 16, fcolors);        GL_DEAD_YET();
}

//} grid_shader_t
//{ blitter_t - fgt.gl.BlitShader, fgt.gl.Blitter
struct blit_shader_t : public shader_t {
    enum blitmode_t : int {
        M_FILL   = 1, // # fill with color
        M_BLEND  = 2, // # texture it
        M_OPAQUE = 3, // # force texture alpha to 1.0
        M_RALPHA = 4, // # fill with color; take alpha from tex
    };
    int u_tex;
    int u_dstsize;
    int u_mode;
    int u_color;
    int u_srcrect;
    int u_srcsize;

    void bind_attribute_locs();
    void get_uniform_locs();

    void set_at_blit(const blit_shader_t::blitmode_t mode,
        const SDL_Rect *dstsize, // destination viewport size in pixels (needed for NDC coords)
        const int r, const int g, const int b, const int a, // color
        const SDL_Rect *srcrect, // source rect in pixels (if partial blit)
        const SDL_Rect *srcsize  // source size in pixels (if partial blit) (needed for texcoords)
    );
};

void blit_shader_t::bind_attribute_locs() {
    glBindAttribLocation(program, 0, "position");

    GL_DEAD_YET();
}

void blit_shader_t::get_uniform_locs() {
    u_tex       = glGetUniformLocation(program, "tex");
    u_dstsize   = glGetUniformLocation(program, "dstsize");
    u_mode      = glGetUniformLocation(program, "mode");
    u_color     = glGetUniformLocation(program, "color");
    u_srcrect   = glGetUniformLocation(program, "srcrect");
    u_srcsize   = glGetUniformLocation(program, "srcsize");

    GL_DEAD_YET();
}

void blit_shader_t::set_at_blit(const blit_shader_t::blitmode_t mode,
        const SDL_Rect *dstsize, // destination viewport size in pixels (needed for NDC coords)
        const int r, const int g, const int b, const int a, // color
        const SDL_Rect *srcrect, // source rect in pixels (if partial blit)
        const SDL_Rect *srcsize  // source size in pixels (if partial blit) (needed for texcoords)
) {

    glUseProgram(program);
    glUniform1i(u_tex, 0);
    glUniform2i(u_dstsize, dstsize->w, dstsize->h);
    glUniform1i(u_mode, mode);
    glUniform4f(u_color, (float)r/255.0, (float)g/255.0, (float)b/255.0, (float)a/255.0);

    if (srcsize && srcrect) {
        glUniform4i(u_srcrect, srcrect->x, srcrect->y, srcrect->w, srcrect->h);
        glUniform2i(u_srcsize, srcsize->w, srcsize->h);
    } else {
        glUniform4i(u_srcrect, -1, -2, -3, -4);
        glUniform2i(u_srcsize, -5, -6);
    }

    GL_DEAD_YET();
}

struct blitter_t {
    blit_shader_t shader;
    GLuint vaoname, bufname;

    void initialize();
    void finalize();

    void fill(
        const SDL_Rect *dstrect, // destination rect in pixels.
        const SDL_Rect *dstsize, // destination viewport size in pixels
        const int r = 0, const int g = 0, const int b = 0, const int a = 0xAD // color
        ) { _blit( blit_shader_t::M_FILL, dstrect, dstsize, r, g, b, a, NULL, NULL ); }
    void blend(
        const SDL_Rect *dstrect, // destination rect in pixels.
        const SDL_Rect *dstsize, // destination viewport size in pixels (needed for NDC coords)
        const SDL_Rect *srcrect = NULL, // source rect in pixels (if partial blit)
        const SDL_Rect *srcsize = NULL // source size in pixels (if partial blit) (needed for texcoords)
        ) { _blit( blit_shader_t::M_BLEND, dstrect, dstsize, 0, 0, 0, 0, srcrect, srcsize ); }
    void opaque(
        const SDL_Rect *dstrect, // destination rect in pixels.
        const SDL_Rect *dstsize, // destination viewport size in pixels (needed for NDC coords)
        const SDL_Rect *srcrect = NULL, // source rect in pixels (if partial blit)
        const SDL_Rect *srcsize = NULL // source size in pixels (if partial blit) (needed for texcoords)
        ) { _blit( blit_shader_t::M_OPAQUE, dstrect, dstsize, 0, 0, 0, 0, srcrect, srcsize ); }

    void ralpha(
        const SDL_Rect *dstrect, // destination rect in pixels.
        const SDL_Rect *dstsize, // destination viewport size in pixels (needed for NDC coords)
        const int r = 0, const int g = 0, const int b = 0, const int a = 0xAD, // color
        const SDL_Rect *srcrect = NULL, // source rect in pixels (if partial blit)
        const SDL_Rect *srcsize = NULL // source size in pixels (if partial blit) (needed for texcoords)
        ) { _blit( blit_shader_t::M_RALPHA, dstrect, dstsize, r, g, b, a, srcrect, srcsize ); }

    void _blit( blit_shader_t::blitmode_t blitmode,
        const SDL_Rect *dstrect, // destination rect in pixels.
        const SDL_Rect *dstsize, // destination viewport size in pixels (needed for NDC coords)
        const int r, const int g, const int b, const int a, // color
        const SDL_Rect *srcrect, // source rect in pixels (if partial blit)
        const SDL_Rect *srcsize  // source size in pixels (if partial blit) (needed for texcoords)
        );
};

void blitter_t::initialize() {
    shader.initialize("blit130");
    glGenVertexArrays(1, &vaoname);
    glGenBuffers(1, &bufname);
    glBindVertexArray(vaoname);
    glBindBuffer(GL_ARRAY_BUFFER, bufname);
    glBufferData(GL_ARRAY_BUFFER, 4096, NULL, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_INT,  GL_FALSE, 0, 0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    GL_DEAD_YET();
}

void blitter_t::finalize() {
    shader.finalize();
    glDeleteVertexArrays(1, &vaoname);
    glDeleteBuffers(1, &bufname);
}

void blitter_t::_blit( blit_shader_t::blitmode_t blitmode,
        const SDL_Rect *dstrect, // destination rect in pixels.
        const SDL_Rect *dstsize, // destination viewport size in pixels (needed for NDC coords)
        const int r, const int g, const int b, const int a, // color
        const SDL_Rect *srcrect, // source rect in pixels (if partial blit)
        const SDL_Rect *srcsize  // source size in pixels (if partial blit) (needed for texcoords)
    ) {
    glBindVertexArray(vaoname);
    glBindBuffer(GL_ARRAY_BUFFER, bufname);

    int *bd = (int *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    GL_DEAD_YET();
    if (!bd)
        platform->fatal("NULL from glMapBuffer() in blitter");

    bd[ 0] = dstrect->x; bd[ 1] = dstrect->y;             // bottom left
        bd[ 2] = 0; bd[ 3] = 1;
    bd[ 4] = dstrect->x + dstrect->w; bd[ 5] = dstrect->y; // bottom right
        bd[ 6] = 1; bd[ 7] = 1;
    bd[ 8] = dstrect->x; bd[ 9] = dstrect->y + dstrect->h; // top left
        bd[10] = 0; bd[11] = 0;
    bd[12] = dstrect->x + dstrect->w; bd[13] = dstrect->y + dstrect->h; // top right
        bd[14] = 1; bd[15] = 0;

    glUnmapBuffer(GL_ARRAY_BUFFER);

    shader.set_at_blit(blitmode, dstsize, r, g, b, a, srcrect, srcsize);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

//} blitter_t

//{ ttf_renderer_t
struct ttf_shader_t : public shader_t {
    int u_text_sampler;
    int u_attr_sampler;
    int u_vp_size;
    int u_text_size;
    int u_colors;

    void bind_attribute_locs() {
        glBindAttribLocation(program, 0, "position");
        glBindAttribLocation(program, 1, "stuff_in");
        GL_DEAD_YET();
    }
    void get_uniform_locs() {
        u_text_sampler    = glGetUniformLocation(program, "text_tex");
        u_attr_sampler    = glGetUniformLocation(program, "attr_tex");
        u_vp_size         = glGetUniformLocation(program, "vp_size");
        u_text_size       = glGetUniformLocation(program, "text_size");
        u_colors          = glGetUniformLocation(program, "colors");
        if (u_colors == -1)
            u_colors = glGetUniformLocation(program, "colors[0]");
        GL_DEAD_YET();
    }
    void set_at_init(const ansi_colors_t *ccolors) {
        GLfloat fcolors[16*4];
        for (int i = 0; i <16 ; i++) {
            fcolors[4*i + 0] = (float) ((*ccolors)[i][0]) / 255.0;
            fcolors[4*i + 1] = (float) ((*ccolors)[i][1]) / 255.0;
            fcolors[4*i + 2] = (float) ((*ccolors)[i][2]) / 255.0;
            fcolors[4*i + 3] = 1.0f;
        }
        glUseProgram(program);
        glUniform4fv(u_colors, 16, fcolors);
        GL_DEAD_YET();
    }
    void set_at_frame(int tu_text, int tu_attr, int vp_w, int vp_h, int tex_w, int tex_h) {

        glUseProgram(program);
        glUniform1i(u_text_sampler, tu_text);
        glUniform1i(u_attr_sampler, tu_attr);
        glUniform2f(u_vp_size, vp_w, vp_h);
        glUniform2f(u_text_size, tex_w, tex_h);

        GL_DEAD_YET();
    }
};

struct ttf_renderer_t {
    const int tu_attr;
    const int tu_text;
    const ansi_colors_t *ccolors;

    ilogger *logr, *renderlogr, *sizerlogr, *blitlogr;
    void initialize(const char *, int, const ansi_colors_t *);
    void finalize();
    int active(int);
    int gridwidth(const uint16_t *, const uint32_t, const uint32_t, uint32_t *, uint32_t *,
                                                          uint32_t *, uint32_t *, uint32_t *);
    void render(df_text_t *, int, int, int, int);

    zhban_t *zhban;
    int lineheight;
    int font_data_len;
    void *font_data;            // can be freeed only after zhban_drop()

    GLuint attr_tex, attr_bo, text_tex, text_bo;
    GLuint vao_name, vao_bo, idx_bo;
    int count_max, wrote;

    ttf_shader_t shader;

    ttf_renderer_t():
        tu_attr(4), tu_text(5),
        ccolors(NULL),
        logr(NULL), renderlogr(NULL), sizerlogr(NULL), blitlogr(NULL),
        zhban(NULL), lineheight(0),
        font_data_len(0), font_data(NULL),
        attr_tex(0), attr_bo(0),
        text_tex(0), text_bo(0),
        vao_name(0), vao_bo(0),
        idx_bo(0), count_max(2048),
        wrote(0),
        shader()
    { }
};
int ttf_renderer_t::active(int lh) {
    return zhban != 0 && lh == lineheight;
}
void ttf_renderer_t::initialize(const char* fname, int lh, const ansi_colors_t *colors) {
    logr = platform->getlogr("ttf");
    logr->info("initialize(%s, %d)", fname, lh);
    renderlogr = platform->getlogr("ttf.render");
    blitlogr = platform->getlogr("ttf.blit");
    sizerlogr = platform->getlogr("ttf.sizer");
    if (lh == 0)
        return; // handle [TRUETYPE:OFF]

    if (zhban)
        logr->fatal("resetting ttf_lineheight from %d to %d", lineheight, lh);

    FILE *fp;
    if (!(fp = fopen(fname, "rb"))) {
        logr->error("can't open '%s': %s", fname, strerror(errno));
        return;
    }
    fseek(fp, 0, SEEK_END);
    font_data_len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    font_data = malloc(font_data_len);
    if (font_data == NULL) {
        logr->error("no memory for %s (%d bytes)", fname, font_data_len);
        return;
    }
    if (1 != fread(font_data, font_data_len, 1, fp)) {
        logr->error("read failed '%s': %s", fname, strerror(errno));
        fclose(fp);
        return;
    }
    fclose(fp);

    int verbose = sizerlogr->enabled(LL_TRACE);
    zhban = zhban_open(font_data, font_data_len, lh, 2*lh, 1<<20, 1<<20, verbose);
    if (zhban == NULL) {
        logr->error("zhban_open() failed.");
        free(font_data);
        return;
    }
    lineheight = lh;

    glGenBuffers(1, &text_bo);
    glGenBuffers(1, &attr_bo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, attr_bo); // best practices
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);       // clowns upon them

    glGenBuffers(1, &vao_bo);
    glGenBuffers(1, &idx_bo);

    glGenTextures(1, &text_tex);
    glGenTextures(1, &attr_tex);

    glBindTexture(GL_TEXTURE_2D, text_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, attr_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenVertexArrays(1, &vao_name);

    glBindVertexArray(vao_name);
    glBindBuffer(GL_ARRAY_BUFFER, vao_bo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idx_bo);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 4, GL_INT, GL_FALSE, 32, 0);
    glVertexAttribIPointer(1, 2, GL_INT, 32, ((char*)NULL)+16);

    /* generate and upload a static index buffer */
    int idx_bo_size = count_max * 5 * sizeof(uint32_t);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx_bo_size, NULL, GL_STATIC_DRAW);
    uint32_t *idxptr = (uint32_t *)glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);

    if (!idxptr)
        logr->fatal("glMapBuffer(idx_bo, size=%d): returned NULL.", idx_bo_size);

    for (int i = 0; i < count_max ; i++) {
        idxptr[5*i + 0] = 4*i + 0;
        idxptr[5*i + 1] = 4*i + 1;
        idxptr[5*i + 2] = 4*i + 2;
        idxptr[5*i + 3] = 4*i + 3;
        idxptr[5*i + 4] = 0xFFFFFFFFu;
    }
    glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
    GL_DEAD_YET();

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    GL_DEAD_YET();

    shader.initialize("ttf130");
    shader.set_at_init(colors);
}

void ttf_renderer_t::finalize() {
    if (zhban) {
        zhban_drop(zhban);
        free(font_data);
        zhban = NULL;
        font_data = NULL;
        lineheight = 0;
    }
}

uint8_t naughty_buffer[4096];
static char *utf16to8(const uint16_t *chars, const uint32_t len) {
    uint8_t *ptr = naughty_buffer;
    uint32_t i = 0;
    while ((len > i) && ((ptr - naughty_buffer) < 4093)) {
        uint16_t c = chars[i++];
        if (c <= 0x7f) {
            *ptr++ = c;
            continue;
        }
        if (c <= 0x7ff) {
            *ptr++ = (c >> 6) | 0xc0;
            *ptr++ = (c & 0x3f) | 0x80;
            continue;
        }
        *ptr++ = (c >> 12) | 0xe0;
        *ptr++ = ((c >> 6) & 0x3f) | 0x80;
        *ptr++ = (c & 0x3f) | 0x80;
    }
    *ptr = 0;
    return (char *)naughty_buffer;
}

int ttf_renderer_t::gridwidth(const uint16_t *chars, const uint32_t len, const uint32_t pszx,
                       uint32_t *w, uint32_t *h, uint32_t *ox, uint32_t *oy, uint32_t *pixel_pad) {
    zhban_rect_t zrect;
    if (zhban_size(zhban, chars, 2*len, &zrect)) {
        sizerlogr->error("zhban_size() failed");
        return len;
    }
    *w = zrect.w; *h = zrect.h;
    *ox = zrect.origin_x; *oy = zrect.origin_y;
    int rv = (zrect.w % pszx) ? zrect.w/pszx + 1 : zrect.w/pszx;
    *pixel_pad = pszx*rv - zrect.w;

    if (sizerlogr->enabled(LL_TRACE))
        sizerlogr->trace("pszx=%d len=%d w=%d rv=%d bytes: \"%s\"",
                            pszx, len, zrect.w, rv, utf16to8(chars, len));

    return rv;
    //return q * pszx < rt.w ? q + 1 : q;
}

static inline uint32_t pot_align(uint32_t value, uint32_t pot) {
    return (value + (1<<pot) - 1) & ~((1<<pot) -1);
}

void ttf_renderer_t::render(df_text_t *text, int pszx, int pszy, int vpw, int vph) {
    int count;
    if ((count = text->size()) == 0)
        return;
    renderlogr->trace("ttf_renderer text=%p count=%d", (void*)text, count);
    if (count > count_max)
        renderlogr->fatal("recompile to allow %d text strings at once (current limit %d)", count, count_max);

    renderlogr->trace("render([%d], %d, %d, %d, %d)", text->size(), pszx, pszy, vpw, vph);
    int i;

    //{ upload attrs
    int attr_bo_size = text->attrs_used;
    int attr_tex_w = 512;
    int attr_tex_h = attr_bo_size / attr_tex_w + 1;
    attr_bo_size = attr_tex_w * attr_tex_h;

    /* the following should be a GL_TEXTURE_BUFFER, but it's 3.1+. */
    /* thus it stays a texture. */
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, attr_bo); // staying strictly 3.0 :(
#if !defined(MALLOC_GLBUFFERS)
    glBufferData(GL_PIXEL_UNPACK_BUFFER, attr_bo_size, NULL, GL_STREAM_DRAW);
    uint8_t *attrptr = (uint8_t *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
#else
    uint8_t *attrptr = (uint8_t *)malloc(attr_bo_size);
#endif
    if (!attrptr)
        renderlogr->fatal("glMapBuffer(attr_bo_size=%d): returned NULL.", attr_bo_size);
    memcpy(attrptr, text->attrs, text->attrs_used);
    /* release the attr texture bo */
#if !defined(MALLOC_GLBUFFERS)
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
#else
    glBufferData(GL_PIXEL_UNPACK_BUFFER, attr_bo_size, attrptr, GL_STREAM_DRAW);
    free(attrptr);
    attrptr = NULL;
#endif

    /* "upload" attr-texture */
    renderlogr->trace("attr_bo_size=%d attr_tex_w=%d attr_tex_h=%d", attr_bo_size, attr_tex_w, attr_tex_h);
    glActiveTexture(GL_TEXTURE0 + tu_attr);
    glBindTexture(GL_TEXTURE_2D, attr_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, attr_tex_w, attr_tex_h, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    GL_DEAD_YET();

    //}
    //{ Make an album texture of rendered text snippets.
    /*
       Since it's an one-off thing, we merge the album index
       into the vertex attributes, just repeating it for
       all vertices of a quad.

       Thus, the VAO backing buffer is filled up here too. */

    int vao_bo_size = count * 4 * 8 * sizeof(int32_t);

        //{ map vao bo
    glBindVertexArray(vao_name);
    glBindBuffer(GL_ARRAY_BUFFER, vao_bo);
#if !defined(MALLOC_GLBUFFERS)
    glBufferData(GL_ARRAY_BUFFER, vao_bo_size, NULL, GL_STREAM_DRAW);
    int32_t *vao = (int32_t *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
#else
    int32_t *vao = (int32_t *)malloc(vao_bo_size);
    int32_t *bdo = vao;
#endif

    if (!vao)
        renderlogr->fatal("glMapBuffer(ttf-vao, size=%d): returned NULL.",vao_bo_size);
        //}
        //{ generate VAO data and texalbum metadata
    std::vector<int> tex_offsets;
    int tex_w = 512, tex_h = 0;
    int texcur_x = 0,texcur_y = 0;     // current posn.
    int row_h = text->zrects[0].h + 1; // first row height: texture + clustermap
    int left, right, top, bottom, gx, gy; // VAO stuff - quad.
    int tex_t, tex_r, tex_l, tex_b;       // VAO stuff - texcoords.
    int zr_w, zr_h, shift;
    for (i = 0; i < count; i++) {
        zr_w = text->zrects[i].w;
        zr_h = text->zrects[i].h;
        if ((tex_w - texcur_x < zr_w) || ( zr_h > row_h))// advance down
            texcur_x = 0, texcur_y += row_h, row_h = zr_h + 1;

        tex_offsets.push_back(texcur_x);
        tex_offsets.push_back(texcur_y);

        tex_l = texcur_x;        tex_t = texcur_y;
        tex_r = texcur_x + zr_w; tex_b = texcur_y + zr_h;

        texcur_x += zr_w;

        gx = text->grid_coords[2*i];
        gy = text->grid_coords[2*i+1];
        shift = text->pixel_offsets[i];
        left   = gx * pszx + shift; right = left + zr_w + shift;
        bottom = gy * pszy; top = bottom + zr_h;

        blitlogr->trace("[%d] grid %d,%d l=%d r=%d b=%d t=%d", i, gx, gy, left, right, bottom, top);
        int fg_attr_offset = text->attr_offsets[i];
        int clustermap_row = texcur_y + zr_h + 1;

        vao[ 0] = left;  vao[ 1] = bottom;  // vertex
        vao[ 2] = tex_l; vao[ 3] = tex_b;   // texcoord
        vao[ 4] = fg_attr_offset;
        vao[ 5] = clustermap_row;
        vao[ 6] = 0;
        vao[ 7] = 0;
        vao += 8;

        vao[ 0] = right;  vao[ 1] = bottom;
        vao[ 2] = tex_r;  vao[ 3] = tex_b;
        vao[ 4] = fg_attr_offset;
        vao[ 5] = clustermap_row;
        vao[ 6] = 0;
        vao[ 7] = 0;
        vao += 8;

        vao[ 0] = left;   vao[ 1] = top;
        vao[ 2] = tex_l;  vao[ 3] = tex_t;
        vao[ 4] = fg_attr_offset;
        vao[ 5] = clustermap_row;
        vao[ 6] = 0;
        vao[ 7] = 0;
        vao += 8;

        vao[ 0] = right;  vao[ 1] = top;
        vao[ 2] = tex_r;  vao[ 3] = tex_t;
        vao[ 4] = fg_attr_offset;
        vao[ 5] = clustermap_row;
        vao[ 6] = 0;
        vao[ 7] = 0;
        vao += 8;
    }
        //}
        //{ unmap vao bo
#if !defined(MALLOC_GLBUFFERS)
    glUnmapBuffer(GL_ARRAY_BUFFER);
#else
    glBufferData(GL_ARRAY_BUFFER, vao_bo_size, bdo, GL_STREAM_DRAW);
    free(bdo);
    bdo = NULL;
#endif
        //}

    tex_h = texcur_y + row_h;
    int tex_bo_size = tex_w * tex_h * 4;  // tex_w*4-aligned, which is 4096
    renderlogr->trace("text_tex %dx%d bo %d bytes", tex_w, tex_h, tex_bo_size);

        //{ map text texture bo
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, text_bo);
#if !defined(MALLOC_GLBUFFERS)
    glBufferData(GL_PIXEL_UNPACK_BUFFER, tex_bo_size, NULL, GL_STREAM_DRAW);
    uint8_t *texptr = (uint8_t *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_READ_WRITE);
#else
    uint8_t *texptr = (uint8_t *)malloc(tex_bo_size);
#endif
    if (!texptr)
        renderlogr->fatal("glMapBuffer(tex_bo_size=%d): returned NULL.", tex_bo_size);
        //}
        //{ render text and copy results into the buffer.
    for (i = 0; i < count; i++) {

        zhban_rect_t *zr = &text->zrects[i];

        if (blitlogr->enabled(LL_TRACE)) {
            char *u8 = utf16to8(text->strings + text->string_offsets[i]/2,
                                text->string_lengths[i]);
            blitlogr->trace("[%d] initial zr sz %dx%d ori %d,%d bytes: \"%s\"",
                    i, zr->w, zr->h, zr->origin_x, zr->origin_y, u8);
        }

        zhban_render(zhban, text->strings + text->string_offsets[i]/2,
                            text->string_lengths[i]*2, zr);

        texcur_x = tex_offsets[2*i];
        texcur_y = tex_offsets[2*i + 1];
        blitlogr->trace("[%d] tex blit %dx%d to %d,%d", i, zr->w, zr->h, texcur_x, texcur_y);
        uint8_t *dest_origin = texptr + 4 * (texcur_x + texcur_y * tex_w);
        for (int j = 0; j < zr->h+1; j++)
            memcpy(dest_origin + j * tex_w * 4, zr->data + j * zr->w, zr->w*4);
    }
        //}
        //{ unmap text texture bo, set up the texture
#if !defined(MALLOC_GLBUFFERS)
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
#else
    glBufferData(GL_PIXEL_UNPACK_BUFFER, tex_bo_size, texptr, GL_STREAM_DRAW);
    free(texptr);
    texptr = NULL;
#endif

    /* "upload" the text-texture */
    glActiveTexture(GL_TEXTURE0 + tu_text);
    glBindTexture(GL_TEXTURE_2D, text_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16UI, tex_w, tex_h, 0, GL_RG_INTEGER , GL_UNSIGNED_SHORT, 0);
    GL_DEAD_YET();
    /* dump it. */
    if (!wrote) { FILE *fp = fopen("tex.dump", "w");
        fwrite(texptr, tex_bo_size, 1, fp);
        fclose(fp);
        wrote=1;
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    GL_DEAD_YET();
        //}

    //}
    /* GL-render */
    shader.set_at_frame(tu_text, tu_attr, vpw, vph, tex_w, tex_h);
    glEnableClientState(GL_PRIMITIVE_RESTART_NV);
    GL_DEAD_YET();
    glPrimitiveRestartIndexNV(0xFFFFFFFFu);
    GL_DEAD_YET();
    glDrawElements(GL_TRIANGLE_STRIP, count*5, GL_UNSIGNED_INT, 0);
    GL_DEAD_YET();
    glDisableClientState(GL_PRIMITIVE_RESTART_NV);
    GL_DEAD_YET();

    /* clean up GL state */
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    GL_DEAD_YET();

    text->reset();
}

//}

/*  OpenGL 3.0 renderer, rewrite of renderer_sdl2gl2. */

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
    void upload_textures();

    void set_gridsize(unsigned w, unsigned h) { grid_w = w, grid_h = h; }
    uint32_t get_gridsize(void) { return (grid_w << 16) | (grid_h & 0xFFFF); }

    df_buffer_t *get_buffer();
    void submit_buffer(df_buffer_t *buf);
    df_buffer_t *get_offscreen_buffer(unsigned w, unsigned h);
    void export_offscreen_buffer(df_buffer_t *buf, const char *name);

    void acknowledge(const itc_message_t&) {}

    int ttf_active();
    int ttf_gridwidth(const uint16_t*, uint32_t, uint32_t*, uint32_t*, uint32_t*,
                                                            uint32_t*, uint32_t*);
    void start();
    void join();
    void run_here();
    void simuloop_quit() { not_done = false; }
    bool started;

    imqd_t incoming_q;
    imqd_t free_buf_q;

    vbstreamer_t grid_streamer;
    grid_shader_t grid_shader;

    blitter_t blitter;

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
    long Pszx, Pszy;                // font native cel dimensions
    int Psz, Psz_native;            // larger dimension of cel 0 at zoom and native.
    int viewport_x, viewport_y;     // viewport tracking
    int viewport_w, viewport_h;     // for mouse coordinate transformation
    int mouse_xw, mouse_yw;         // mouse window coordinates
    int mouse_xg, mouse_yg;         // mouse grid coordinates
    GLuint fonttex;
    GLuint findextex;
    bool cmd_zoom_in, cmd_zoom_out, cmd_zoom_reset, cmd_tex_reset;
    ilogger *logr, *nputlogr, *rshlogr;
    void export_buffer(df_buffer_t *buf, const char *name);
    const ansi_colors_t *colors;
    df_texalbum_t *album;           // private to upload_textures() method.

    ttf_renderer_t ttf;
};

void implementation::export_buffer(df_buffer_t *buf, const char *name) {
    logr->trace("exporting offscreen buffer %dx%d into %s", buf->w, buf->h, name);
    std::string fname(name);
    if (fname.substr(fname.size() - 4, 4) == ".bmp")
        fname = fname.substr(0, fname.size() - 4);
    logr->trace("%s %s", fname.substr(fname.size() - 4, 4).c_str(), fname.substr(0, fname.size() - 4).c_str() );
    std::string dumpfname(fname);
    dumpfname += ".dump";
    dump_buffer_t(buf, dumpfname.c_str());
    logr->trace("raw buffer dump written");
    /* okay, so what do we do here.

        - work in 1024x1024 chunks, should be supported much anywhere.
            - set up framebuffer with such chunk attached as target, PBO.
            - set up shaders to draw just the corresponding part
            - take care to split parts on tile boundary, or some GL_POINTS
              might get clipped, and they are clipped totally.
            - fetch pixel data, SDL-blit it into the resulting surface
        - then call SDL_SavePNG on it
    */
    /* first take would be just a single pass, error out if GL can't handle the size */
    /* and the size is calculated from the first celpage */
    int vp_w = buf->w * Pszx;
    int vp_h = buf->h * Pszy;

    GLuint fb_name, rb_name, va_name, bo_name;
    glGenFramebuffers(1, &fb_name);
    glGenRenderbuffers(1, &rb_name);
    glGenVertexArrays(1, &va_name);
    glGenBuffers(1, &bo_name);
    glBindFramebuffer(GL_FRAMEBUFFER, fb_name);
    glBindRenderbuffer(GL_RENDERBUFFER, rb_name);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, vp_w, vp_h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb_name);
    GL_DEAD_YET();
    if (GL_FRAMEBUFFER_COMPLETE != glCheckFramebufferStatus(GL_FRAMEBUFFER)) {
        logr->fatal("framebuffer incomplete");
    }
    glViewport(0, 0, vp_w, vp_h);
    glClearColor(0.4, 0.8, 0.4, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    GL_DEAD_YET();

    // VAO & BO setup copied from the vbstreamer
    glBindVertexArray(va_name);
    glBindBuffer(GL_ARRAY_BUFFER, bo_name);
    glEnableVertexAttribArray(grid_streamer.screen_posn);
    glVertexAttribIPointer(grid_streamer.screen_posn,    4, GL_UNSIGNED_BYTE,  0, DFBUFOFFS(buf, screen));
    glEnableVertexAttribArray(grid_streamer.texpos_posn);
    glVertexAttribIPointer(grid_streamer.texpos_posn,    1, GL_UNSIGNED_INT,   0, DFBUFOFFS(buf, texpos));
    glEnableVertexAttribArray(grid_streamer.addcolor_posn);
    glVertexAttribIPointer(grid_streamer.addcolor_posn,  1, GL_UNSIGNED_BYTE,  0, DFBUFOFFS(buf, addcolor));
    glEnableVertexAttribArray(grid_streamer.grayscale_posn);
    glVertexAttribIPointer(grid_streamer.grayscale_posn, 1, GL_UNSIGNED_BYTE,  0, DFBUFOFFS(buf, grayscale));
    glEnableVertexAttribArray(grid_streamer.cf_posn);
    glVertexAttribIPointer(grid_streamer.cf_posn,        1, GL_UNSIGNED_BYTE,  0, DFBUFOFFS(buf, cf));
    glEnableVertexAttribArray(grid_streamer.cbr_posn);
    glVertexAttribIPointer(grid_streamer.cbr_posn,       1, GL_UNSIGNED_BYTE,  0, DFBUFOFFS(buf, cbr));
    glEnableVertexAttribArray(grid_streamer.fx_posn);
    glVertexAttribIPointer(grid_streamer.fx_posn,        1, GL_UNSIGNED_BYTE,  0, DFBUFOFFS(buf, fx));
    glBufferData(GL_ARRAY_BUFFER, buf->required_sz, buf->ptr, GL_STREAM_DRAW);
    GL_DEAD_YET();

    grid_shader.set_at_resize(Parx, Pary, Psz_native, buf->w, buf->h);
    grid_shader.set_at_frame(1.0);
    GL_DEAD_YET();

    glDrawArrays(GL_POINTS, 0, buf->w * buf->h);
    GL_DEAD_YET();
    glFinish();

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    GL_DEAD_YET();
    Uint32 Rmask, Bmask, Gmask, Amask; int bpp;
    SDL_PixelFormatEnumToMasks(SDL_PIXELFORMAT_RGBA8888, &bpp, &Rmask, &Bmask, &Gmask, &Amask);
    SDL_Surface *surf = SDL_CreateRGBSurface(0, vp_w, vp_h, bpp, Rmask, Bmask, Gmask, Amask);

    /* flip while reading */
    for (int i = 0; i < vp_h ; i++)
        glReadPixels(0, i, vp_w, 1, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
            ((uint8_t *)surf->pixels) + surf->pitch * (vp_h - i - 1));

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteRenderbuffers(1, &rb_name);
    glDeleteFramebuffers(1, &fb_name);
    glDeleteVertexArrays(1, &va_name);
    glDeleteBuffers(1, &bo_name);

    dumpfname = fname;
    dumpfname += ".png";
    SDL_SavePNG(surf, dumpfname.c_str());
    SDL_FreeSurface(surf);

    /* restore gridshader and viewport state */
    grid_shader.set_at_resize(Parx, Pary, Psz, grid_w, grid_h);
    glViewport(viewport_x, viewport_y, viewport_w, viewport_h);
}

void implementation::initialize() {
    nputlogr = platform->getlogr("sdl.input");
    rshlogr = platform->getlogr("sdl.reshape");
    ilogger *glclogr = platform->getlogr("gl.context");

    /* this GL renderer needs or uses no SDL internal rendering
       or even a window surface. */
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "0");

    /* hmm. platform does only defaults+timers? or video too?
       but PM:2D might want accelerated render driver.
       and InitSubsystem() calls VideoInit(NULL). Okay,
       let's just InitSubsystem() and see what happens. */
    if (SDL_InitSubSystem(SDL_INIT_VIDEO))
        logr->fatal("SDL_InitSubsystem(SDL_INIT_VIDEO): %s", SDL_GetError());

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
#if 1
        {SDL_GL_CONTEXT_MAJOR_VERSION, "SDL_GL_CONTEXT_MAJOR_VERSION", 3},
        {SDL_GL_CONTEXT_MINOR_VERSION, "SDL_GL_CONTEXT_MINOR_VERSION", 0}
#else
        {SDL_GL_CONTEXT_MAJOR_VERSION, "SDL_GL_CONTEXT_MAJOR_VERSION", 3},
        {SDL_GL_CONTEXT_MINOR_VERSION, "SDL_GL_CONTEXT_MINOR_VERSION", 1},
        {SDL_GL_CONTEXT_PROFILE_MASK, "SDL_GL_CONTEXT_PROFILE_MASK", SDL_GL_CONTEXT_PROFILE_CORE},
        {SDL_GL_CONTEXT_FLAGS, "SDL_GL_CONTEXT_FLAGS", 0}
#endif
    };

    for (unsigned i=0; i < sizeof(attr_req)/sizeof(gl_attr); i++) {
        if (SDL_GL_SetAttribute(attr_req[i].attr, attr_req[i].value))
            logr->fatal("SDL_GL_SetAttribute(%s, %d): %s",
                attr_req[i].name, attr_req[i].value, SDL_GetError());
    }

    int window_w, window_h;
    {
        const char *init_font = platform->get_setting("init.FONT", "curses_640x300.png");
        std::string fontpath = init_font;
        if (!strchr(init_font, '/') && !strchr(init_font, '\\')) {
            fontpath = "data/art/";
            fontpath.append(init_font);
        }
        logr->info("loading '%s'", fontpath.c_str());

        /* drop whatever crap has been loaded before that */
        textures->reset();
        /* Note the below sets Pszx and Pszy. This is done once, here. */
        long unused[256];
        textures->load_multi_pdim(fontpath.c_str(), unused, 16, 16, true, &Pszx, &Pszy);

        int init_w = atoi(platform->get_setting("init.WINDOWEDX", "80"));
        int init_h = atoi(platform->get_setting("init.WINDOWEDY", "25"));
        window_w = init_w < MAX_GRID_X ? init_w * Pszx : init_w;
        window_h = init_h < MAX_GRID_Y ? init_h * Pszy : init_h;
        logr->info("window %dx%d init %dx%d", window_w, window_h, init_w, init_h);
    }

    gl_window = SDL_CreateWindow("Dwarf Fortress / sdl2gl3",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        window_w, window_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (!gl_window)
        logr->fatal("SDL_CreateWindow(...): %s", SDL_GetError());

    gl_context = SDL_GL_CreateContext(gl_window);

    if (!gl_context)
        logr->fatal("SDL_GL_CreateContext(...): %s", SDL_GetError());

    for (unsigned i=0; i < sizeof(attr_req)/sizeof(gl_attr); i++) {
        int value;
        if (SDL_GL_GetAttribute(attr_req[i].attr, &value))
            logr->fatal("SDL_GL_GetAttribute(%s, ptr): %s",
                attr_req[i].name, SDL_GetError());
        glclogr->info("%s requested %d got %d",
            attr_req[i].name, attr_req[i].value, value);
    }
    glclogr->info("GL version %s", glGetString(GL_VERSION));
    glclogr->info("GL renderer %s", glGetString(GL_RENDERER));
    glclogr->info("GLSL version %s",glGetString(GL_SHADING_LANGUAGE_VERSION));

    GLenum err = glewInit();
    if (GLEW_OK != err)
        glclogr->fatal("glewInit(): %s", glewGetErrorString(err));

    if (!glFenceSync)
        glclogr->fatal("ARB_sync extension or OpenGL 3.2+ is required.");

    if (!glPrimitiveRestartIndexNV)
        glclogr->fatal("NV_primitive_restart extension or OpenGL 3.1+ is required.");
#if 0
    /* this is not present in glew 1.6. and it ain't that fatal anyway */
    if (!GLEW_ARB_map_buffer_alignment)
        glclogr->fatal("ARB_map_buffer_alignment extension or OpenGL 4.2+ is required.");
#endif
    /* todo:

    glGetInteger(GL_MAJOR_VERSION);
    glGetInteger(GL_MINOR_VERSION);
    glGetInteger(GL_CONTEXT_FLAGS); */

    /* set up whatever static state we need */
    glEnable(GL_POINT_SPRITE);
    glDisable(GL_POINT_SMOOTH); //noop?
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
        GLint param = GL_UPPER_LEFT;
        glPointParameteriv(GL_POINT_SPRITE_COORD_ORIGIN, &param);

    glGenTextures(1, &fonttex);
    glGenTextures(1, &findextex);

    GL_DEAD_YET();

    /* since rendering thread is started after init.begin() has been called
       and thus init.txt/colors.txt have been already run, slurp the setting here. */
    colors = platform->get_colors();

    grid_streamer.initialize();
    grid_shader.initialize("grid130");
    //blitter.initialize();
    {
        const char *fontfile = platform->get_setting("init.TRUETYPE_FONT", "data/art/font.ttf");
        const char *truetype = platform->get_setting("init.TRUETYPE", "NO");
        int lineheight = 0;
        if (0 != strcmp(truetype, "NO"))
            lineheight = atoi(truetype);
        ttf.initialize(fontfile, lineheight, colors);
    }

    cmd_zoom_in = cmd_zoom_out = cmd_zoom_reset = false;
    cmd_tex_reset = true;

    /* calculate base font cel aspect ratios */
    if (Pszx > Pszy) {
        Parx = 1.0;
        Pary = (float)Pszy/(float)Pszx;
        Psz = Psz_native = Pszx;
    } else {
        Parx = (float)Pszx/(float)Pszy;
        Pary = 1.0;
        Psz = Psz_native = Pszy;
    }
    upload_textures();
    reshape(-1, -1, Psz_native);
}

void implementation::finalize() {
    glDeleteTextures(1, &fonttex);
    glDeleteTextures(1, &findextex);
    grid_shader.finalize();
    grid_streamer.finalize();
    ttf.finalize();
}

void implementation::upload_textures() {
    if (album)
        textures->release_album(album);
    album = textures->get_album();
    if (!album)
        logr->fatal("upload_textures(): no album from itextures");

    unsigned font_w, font_h, findex_w, findex_h;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fonttex);
    font_w = album->album->w;
    font_h = album->height;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, font_w, font_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, album->album->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    GL_DEAD_YET();

    /* GL_RGBA16UI: x, y, w|magentic , h|gray (leftmost bit, that is, negative.) */
    findex_w = 128;
    findex_h = album->count/findex_w + 1; // todo: pot_align(sqrt(album->count))
    findex_h = 128;
    if (album->count > findex_w * findex_h)
        logr->fatal("whoa, index too large. %d > %d * %d", album->count, findex_w, findex_h);
    const unsigned findex_sz = findex_w * findex_h * 8;

    int16_t *data = new int16_t[findex_w * findex_h * 4];
    memset(data, 0, findex_sz);

    for (uint32_t i = 0; i < album->count; i++) {
        data[4*i + 0] = album->index[i].rect.x;
        data[4*i + 1] = album->index[i].rect.y;
        data[4*i + 2] = album->index[i].rect.w * ( album->index[i].magentic ? -1 : 1 );
        data[4*i + 3] = album->index[i].rect.h * ( album->index[i].gray ? -1 : 1 );
    }

    if (logr->enabled(LL_TRACE)) {
        std::fstream fid("findex.dump", std::ios::binary | std::ios::out);
        fid.write((char*)data, findex_sz);
        fid.close();
    }

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, findextex);
    GL_DEAD_YET();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16I, findex_w, findex_h, 0, GL_RGBA_INTEGER , GL_SHORT, data);
    GL_DEAD_YET();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL_DEAD_YET();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GL_DEAD_YET();
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GL_DEAD_YET();
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GL_DEAD_YET();

    delete []data;

    GL_DEAD_YET();
    logr->info("upload_textures(): primary cel %dx%d font %dx%d findex %dx%d",
                        album->index[1].rect.w, album->index[1].rect.h,
                        font_w, font_h, findex_w, findex_h, album->count);

    grid_shader.set_at_font(0, 1, colors);
}

DFKeySym translate_sdl2_sym(SDL_Keycode sym) { return (DFKeySym) sym; }
uint16_t translate_sdl2_mod(uint16_t mod) { return mod; }

void implementation::slurp_keys() {
    /*  Have to emulate SDL 1.2 behaviour wrt .unicode events for now.

        Emulation consists of waiting to submit keydown events in
        case a TextEvent arrives next, in which case it gets used
        to set the df_input_event_t::unicode field.

        In case some other event arrives, and is not ignored,
        keydown gets submitted without corresponding unicode value.
        If TextInput arrives while there's no keydown event waiting
        for it, it gets discarded. Since there's no guarantees on
        order of keydown/textinput event arrival, this means that
        the input can be lost.

        In other words, keybinding stuff has to be reworked.
    */
    SDL_Event sdl_event;
    df_input_event_t df_event;
    df_input_event_t df_keydown;
    uint32_t now = platform->GetTickCount();
    bool keydown_waiting = false;
    bool simulate_wheel_release = false;

    const df_input_event_t df_empty_event = {
        now,
        df_input_event_t::DF_TNONE,
        df_input_event_t::DF_BNONE,
        0, DFKS_UNKNOWN, 0, 0, 0, true };

    df_keydown = df_empty_event;

    while (SDL_PollEvent(&sdl_event)) {
        df_event = df_empty_event;
        switch(sdl_event.type) {
        case SDL_TEXTINPUT:
            if (!keydown_waiting) {
                nputlogr->trace("SDL_TEXTINPUT: ignoring '%s': no keydown event waiting.",
                                                                        sdl_event.text.text);
            } else {
                uint32_t codepoint = 0, state = 0;
                uint8_t *s = (uint8_t *)sdl_event.text.text;
                while (utf8decode(&state, &codepoint, *s++));
                df_keydown.unicode = codepoint;
                nputlogr->trace("SDL_TEXTINPUT: Submitting waiting keydown.");
                nputlogr->trace("DF_KEY_DOWN: sym=%x mod=%hx uni=%x", df_keydown.sym,
                                                        df_keydown.mod, df_keydown.unicode);
                simuloop->add_input_event(&df_keydown);
                df_keydown = df_empty_event;
                keydown_waiting = false;
            }
            continue;
        case SDL_KEYDOWN:
            nputlogr->trace("SDL_KEYDOWN: %s: sym=%x mod=%hx uni=%x",
                                SDL_GetKeyName(sdl_event.key.keysym.sym), sdl_event.key.keysym.sym,
                                        sdl_event.key.keysym.mod, sdl_event.key.keysym.unicode);
            if (keydown_waiting) {
                nputlogr->trace("SDL_KEYDOWN: Submitting waiting keydown.");
                nputlogr->trace("DF_KEY_DOWN: sym=%x mod=%hx uni=%x", df_keydown.sym,
                                                        df_keydown.mod, df_keydown.unicode);
                simuloop->add_input_event(&df_keydown);
                df_keydown = df_empty_event;
            }
            df_keydown.type = df_input_event_t::DF_KEY_DOWN;
            df_keydown.sym = translate_sdl2_sym(sdl_event.key.keysym.sym);
            df_keydown.mod = translate_sdl2_mod(sdl_event.key.keysym.mod);
            keydown_waiting = true;

            /* kill switch for testing */
            if (sdl_event.key.keysym.sym == SDLK_F12)
                nputlogr->fatal("aborted by user"); // aw, this is going to hurt

            continue;
        case SDL_KEYUP:
            nputlogr->trace("SDL_KEYUP: %s: sym=%x mod=%hx uni=%x",
                                SDL_GetKeyName(sdl_event.key.keysym.sym), sdl_event.key.keysym.sym,
                                        sdl_event.key.keysym.mod, sdl_event.key.keysym.unicode);
            df_event.type = df_input_event_t::DF_KEY_UP;
            df_event.sym = translate_sdl2_sym(sdl_event.key.keysym.sym);
            df_event.mod = translate_sdl2_mod(sdl_event.key.keysym.mod);
            df_event.unicode = sdl_event.key.keysym.unicode;
            nputlogr->trace("DF_KEY_UP: sym=%x mod=%hx uni=%x", df_event.sym,
                                                    df_event.mod, df_event.unicode);
            break;
        case SDL_MOUSEMOTION:
            mouse_xw = sdl_event.motion.x;
            mouse_yw = sdl_event.motion.y;
            {
                int _xg = (sdl_event.motion.x - viewport_x) / Pszx;
                int _yg = (sdl_event.motion.y - viewport_y) / Pszy;
                if ((_xg != mouse_xg) || (_yg != mouse_yg)) {
                    df_event.type = df_input_event_t::DF_MOUSE_MOVE;
                    mouse_xg = df_event.grid_x = _xg;
                    mouse_yg = df_event.grid_y = _yg;
                    nputlogr->trace("MOUSEMOTION: passing %d,%d -> %d,%d  (%d,%d)",
                        mouse_xw, mouse_yw, _xg, _yg, mouse_xg, mouse_yg);
                    break;
                } else {
                    nputlogr->trace("MOUSEMOTION: ignoring %d,%d -> %d,%d (%d,%d)",
                        mouse_xw, mouse_yw, _xg, _yg, mouse_xg, mouse_yg);
                }
            }
            continue;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            switch (sdl_event.button.button) {
            case SDL_BUTTON_LEFT:
                df_event.button = df_input_event_t::DF_BUTTON_LEFT;
                break;
            case SDL_BUTTON_MIDDLE:
                df_event.button = df_input_event_t::DF_BUTTON_MIDDLE;
                break;
            case SDL_BUTTON_RIGHT:
                df_event.button = df_input_event_t::DF_BUTTON_RIGHT;
                break;
            case SDL_BUTTON_X1:
            case SDL_BUTTON_X2:
            default:
                nputlogr->trace("SDL_MOUSEBUTTON%s: %d, skipped.",
                    sdl_event.type == SDL_MOUSEBUTTONDOWN ? "DOWN": "UP", sdl_event.button.button);
                continue;
            }
            nputlogr->trace("SDL_MOUSEBUTTON%s: %d",
                    sdl_event.type == SDL_MOUSEBUTTONDOWN ? "DOWN": "UP", sdl_event.button.button);

            df_event.type = sdl_event.type == SDL_MOUSEBUTTONDOWN ?
                df_input_event_t::DF_BUTTON_DOWN : df_input_event_t::DF_BUTTON_UP;
            df_event.grid_x = mouse_xg;
            df_event.grid_y = mouse_yg;
            break;
        case SDL_MOUSEWHEEL:
            nputlogr->trace("SDL_MOUSEWHEEL: x=%d y=%d", sdl_event.wheel.x, sdl_event.wheel.y);
            if (sdl_event.wheel.y < 0) {
                df_event.type = df_input_event_t::DF_BUTTON_DOWN;
                df_event.button = df_input_event_t::DF_WHEEL_UP;
                simulate_wheel_release = true;
            } else if (sdl_event.wheel.y > 0) {
                df_event.type = df_input_event_t::DF_BUTTON_DOWN;
                df_event.button = df_input_event_t::DF_WHEEL_DOWN;
                simulate_wheel_release = true;
            } else if (sdl_event.wheel.y == 0) {
                continue;
            }
            df_event.grid_x = mouse_xg;
            df_event.grid_y = mouse_yg;
            break;
        case SDL_WINDOWEVENT:
            switch (sdl_event.window.event) {
            case SDL_WINDOWEVENT_FOCUS_GAINED:
            case SDL_WINDOWEVENT_FOCUS_LOST:
            case SDL_WINDOWEVENT_SHOWN:
            case SDL_WINDOWEVENT_HIDDEN:
            case SDL_WINDOWEVENT_EXPOSED:
            case SDL_WINDOWEVENT_ENTER:
            case SDL_WINDOWEVENT_LEAVE:
            case SDL_WINDOWEVENT_MOVED:
            case SDL_WINDOWEVENT_CLOSE:
            case SDL_WINDOWEVENT_RESTORED:
            case SDL_WINDOWEVENT_MINIMIZED:
            case SDL_WINDOWEVENT_MAXIMIZED:
                continue;
            case SDL_WINDOWEVENT_RESIZED:
                reshape(sdl_event.window.data1, sdl_event.window.data2, -1);
                break;
            default:
                break;
            }
            nputlogr->trace("SDL_WINDOWEVENT %d (%d,%d)",
                sdl_event.window.event, sdl_event.window.data1, sdl_event.window.data2);
            continue;
        case SDL_QUIT:
            nputlogr->trace("SDL_QUIT");
            df_event.type = df_input_event_t::DF_QUIT;
            break;

        default:
            nputlogr->trace("SDL event type=%d ignored.", sdl_event.type);
            continue;
        }
        if (keydown_waiting) {
            simuloop->add_input_event(&df_keydown);
            keydown_waiting = false;
            df_keydown = df_empty_event;
        }

        if (simulate_wheel_release) {
            df_input_event_t wheel_release = df_event;
            wheel_release.type = df_input_event_t::DF_BUTTON_UP;
            simuloop->add_input_event(&df_event);
            simuloop->add_input_event(&wheel_release);
            simulate_wheel_release = false;
        } else {
            simuloop->add_input_event(&df_event);
        }

        df_event = df_empty_event;
    }

    /* submit any waiting keydown since we're out of events */
    if (keydown_waiting) {
        nputlogr->trace("slurp_keys final: Submitting waiting keydown.");
        nputlogr->trace("DF_KEY_DOWN: sym=%x mod=%hx uni=%x", df_keydown.sym,
                                                df_keydown.mod, df_keydown.unicode);
        simuloop->add_input_event(&df_keydown);
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
            logr->warn("got cmd_tex_reset");
            upload_textures();
        }

        int rv;
        df_buffer_t *tbuf;
        while ((tbuf = grid_streamer.get_a_buffer()) != NULL) {
            if ((rv = mqueue->copy(free_buf_q, (void *)&tbuf, sizeof(void *), -1)))
                logr->fatal("%s: %d from mqueue->copy()", __func__, rv);
            else
                tbuf->pstate = BS_FREE_Q,
                logr->trace("placed buf %d (%p) into free_buf_q", grid_streamer.find(tbuf), tbuf);
        }

        df_buffer_t *buf = NULL;
        int read_timeout_ms = 10;
        while (true) {
            void *vbuf; size_t vlen;
            rv = mqueue->recv(incoming_q, &vbuf, &vlen, read_timeout_ms);
            read_timeout_ms = 0;

            if (rv == IMQ_TIMEDOUT)
                break;

            if (rv != IMQ_OK) {
                logr->error("render_thread(): %d from mqueue->recv(); read_timeout=%d.", rv, read_timeout_ms);
                break;
            }

            itc_message_t *msg = (itc_message_t *)vbuf;

            switch (msg->t) {
                case itc_message_t::offscreen_buffer:
                    export_buffer(msg->d.buffer, (char *)msg->d.buffer->tail);
                    free_buffer_t(msg->d.buffer);
                    break;
                case itc_message_t::render_buffer:
                    if (buf) {
                        logr->info("dropped frame (buf %d/%p)", grid_streamer.find(buf), buf);
                        dropped_frames ++;
                        /* drop any text there so it doesn't build up */
                        if (buf->text)
                            ((df_text_t *)buf->text)->reset();
                        grid_streamer.remap_buf(buf);
                    }
                    buf = msg->d.buffer;
                    mqueue->free(msg);
                    break;
                default:
                    logr->error("render_thread(): unknown message type %d", msg->t);
                    break;
            }
        }
        if (buf) {
            if (rshlogr->enabled(LL_TRACE))
                glClearColor(0.25, 0.75, 0.25, 1.0);
            else
                glClearColor(0.0, 0.0, 0.0, 1.0);
            glClear(GL_COLOR_BUFFER_BIT);
            grid_shader.set_at_frame(1.0);
            grid_streamer.draw(buf);
#if 0
            if (0) {
                SDL_Rect dst, dstsz;
                dst.x = 0, dst.y = 0, dst.w = 300, dst.h = 150;
                dstsz.w = viewport_w, dstsz.h = viewport_h;

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, fonttex);
                blitter.opaque(&dst, &dstsz);
                //blitter.fill(&dst, &dstsz, 255,0,255,255);
            }
#endif

            if (buf->text && ttf_active())
                logr->trace("ttf_render(buf=%p)", (void*)buf);
                ttf.render((df_text_t *)(buf->text),
                           (int)(Psz*Parx), (int)(Psz*Pary),
                            viewport_w, viewport_h);

            SDL_GL_SwapWindow(gl_window);
            logr->trace("swap");
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
    rshlogr->trace("reshape(): got window %dx%d psz %d par=%.4fx%.4f",
        new_window_w, new_window_h, new_psz, Parx, Pary);

    if ( (new_window_w > 0) && (new_window_h > 0) ) { // a resize
        if (new_psz > 0)
            platform->fatal("reshape(): reshape + zoom : can't");
        new_psz = Psz;
    } else { // a zoom
        SDL_GetWindowSize(gl_window, &new_window_w, &new_window_h);
    }

    int new_grid_w = new_window_w / (new_psz * Parx);
    int new_grid_h = new_window_h / (new_psz * Pary);

    rshlogr->trace("reshape(): win_wh=%dx%d pszxy= %fx%f new_grid_wh=%dx%d",
        new_window_w, new_window_h, Psz * Parx, Psz * Pary, new_grid_w, new_grid_h);

    new_grid_w = MIN(MAX(new_grid_w, MIN_GRID_X), MAX_GRID_X);
    new_grid_h = MIN(MAX(new_grid_h, MIN_GRID_Y), MAX_GRID_Y);

    viewport_w = lrint(new_grid_w * new_psz * Parx);
    viewport_h = lrint(new_grid_h * new_psz * Pary);

    rshlogr->trace("reshape(): clamped_grid_wh: %dx%d; res vp %dx%d",
                        new_grid_w, new_grid_h, viewport_w, viewport_h);

    bool force_window_w = false;
    bool force_window_h = false;

    if (viewport_w > new_window_w)
        force_window_w = true;
    else
        viewport_x = ( new_window_w - viewport_w ) / 2;

    if (viewport_h > new_window_h)
        force_window_h = true;
    else
        viewport_y = ( new_window_h - viewport_h ) / 2;

    if (force_window_w) {
        if (force_window_h) { /* both */
            SDL_SetWindowSize(gl_window, viewport_w, viewport_h);
            viewport_x = viewport_y = 0;
        } else { /* w but not h */
            SDL_SetWindowSize(gl_window, viewport_w, new_window_h);
            viewport_x = 0;
            viewport_y = ( new_window_h - viewport_h ) / 2;
        }
    } else {
        if (force_window_h) { /* h but not w */
            SDL_SetWindowSize(gl_window, new_window_w, viewport_h);
            viewport_y = ( new_window_h - viewport_h ) / 2;
            viewport_y = 0;
        } else { /* neither */
            viewport_x = ( new_window_w - viewport_w ) / 2;
            viewport_y = ( new_window_h - viewport_h ) / 2;
        }
    }

    glViewport(viewport_x, viewport_y, viewport_w, viewport_h);

    Psz = new_psz;
    grid_w = new_grid_w, grid_h = new_grid_h;
    grid_streamer.set_grid(grid_w, grid_h);

    grid_shader.set_at_resize(Parx, Pary, Psz, grid_w, grid_h);

    rshlogr->trace("reshape(): reshaped to vp %dx%d+%d+%d grid %dx%d psz %d par %.4fx%.4f",
                        viewport_w, viewport_h, viewport_x, viewport_y,
                        grid_w, grid_h, Psz, Parx, Pary);

}

/* Below go methods intended for other threads */

/* return value of NULL means: skip that render_things() call. */
df_buffer_t *implementation::get_buffer(void) {
    df_buffer_t **msg;
    df_buffer_t *buf = NULL;
    size_t len = sizeof(void *);
    int rv;
    switch (rv = mqueue->recv(free_buf_q, (void **)&msg, &len, 0)) {
        /*  wish I could get myself to make this simpler ...
            ok, give recv() a pointer to (void *) and a pointer to size_t
            it writes there a pointer to the message, and message length.
            message here is itself a pointer.
        */

        case IMQ_OK:
            buf = (df_buffer_t *)(*msg);
            mqueue->free(msg);
            buf->pstate = BS_IN_SIMULOOP;
            return buf;
        case IMQ_TIMEDOUT:
            return NULL;
        default:
            logr->fatal("%s: %d from mqueue->recv()", __func__, rv);
    }
    return buf;
}

void implementation::zoom_in() { cmd_zoom_in = true; }
void implementation::zoom_out() { cmd_zoom_out = true; }
void implementation::zoom_reset() { cmd_zoom_reset = true;  }
void implementation::toggle_fullscreen() { logr->trace("toggle_fullscreen(): stub"); }
void implementation::override_grid_size(unsigned, unsigned)  { logr->trace("override_grid_size(): stub"); }
void implementation::release_grid_size() { logr->trace("release_grid_size(): stub"); }
void implementation::reset_textures() { cmd_tex_reset = true; }
int implementation::ttf_active() { return ttf.active(Psz*Pary); }
int implementation::ttf_gridwidth(const uint16_t *c, const unsigned cc, uint32_t *w, uint32_t *h,
                                                 uint32_t *ox,  uint32_t *oy, uint32_t *pixel_pad) {
    return ttf.gridwidth(c, cc, Parx*Psz, w, h, ox, oy, pixel_pad);
}
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
    album = NULL;
    mouse_xg = mouse_yg = 0;
    if ((incoming_q = mqueue->open("renderer", 1<<10)) < 0)
        logr->fatal("%s: %d from mqueue->open(renderer)", __func__, incoming_q);

    if ((free_buf_q = mqueue->open("free_buffers", 1<<10)) < 0)
        logr->fatal("%s: %d from mqueue->open(free_buffers)", __func__, free_buf_q);

    logr = platform->getlogr("sdl2gl3");
}

/* Below is code copied from renderer_ncurses.
   All curses-specific code and all comments were
   ripped out. All SDL/GL specific code is to be
   put into other methods or helper functions. */

//{  All those methods want to go into a parent class.

df_buffer_t *implementation::get_offscreen_buffer(unsigned w, unsigned h) {
    /* tail_sizeof of 1 provides us with w*h bytes where to store export name
       (see below) */
    df_buffer_t *rv = allocate_buffer_t(w, h, 1, 3);
    memset_buffer_t(rv, 0);
    return rv;
}

void implementation::export_offscreen_buffer(df_buffer_t *buf, const char *name) {
    logr->info("exporting buf %p to %s", buf, name);
    strncpy((char *)buf->tail, name, buf->w * buf->h - 1);
    itc_message_t msg;
    memset(&msg, 0, sizeof(msg)); // appease its valgrindiness
    buf->pstate = BS_INBOUND_Q;
    msg.t = itc_message_t::offscreen_buffer;
    msg.d.buffer = buf;
    mqueue->copy(incoming_q, &msg, sizeof(itc_message_t), -1);
}

void implementation::submit_buffer(df_buffer_t *buf) {
    itc_message_t msg;
    memset(&msg, 0, sizeof(msg)); // appease its valgrindiness
    buf->pstate = BS_INBOUND_Q;
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
        logr->error("second renderer start ignored\n");
        return;
    }
    started = true;
    initialize();
    renderer_thread();
}

void implementation::start() {
    if (started) {
        logr->error("second renderer start ignored\n");
        return;
    }
    started = true;
    logr->info("renderer start: this is %p", this);
    thread_id = platform->thread_create(thread_stub, "renderer", (void *)this);
}

void implementation::join() {
    if (!started) {
        logr->error("irenderer::join(): not started\n");
        return;
    }
    platform->thread_join(thread_id, NULL);
}

//}

void implementation::release(void) { }

/* module glue */

static implementation *impl = NULL;

getmqueue_t   _getmqueue = NULL;
gettextures_t _gettextures = NULL;
getsimuloop_t _getsimuloop = NULL;

extern "C" DFM_EXPORT void DFM_APIEP dependencies(
                            getplatform_t   **pl,
                            getmqueue_t     **mq,
                            getrenderer_t   **rr,
                            gettextures_t   **tx,
                            getsimuloop_t   **sl,
                            getmusicsound_t **ms,
                            getkeyboard_t   **kb) {
    *pl = &_getplatform;
    *mq = &_getmqueue;
    *rr = NULL;
    *tx = &_gettextures;
    *sl = &_getsimuloop;
    *ms = NULL;
    *kb = NULL;
}
extern "C" DFM_EXPORT irenderer * DFM_APIEP getrenderer(void) {
    if (!impl) {
        platform = _getplatform();
        mqueue   = _getmqueue();
        textures = _gettextures();
        simuloop = _getsimuloop();
        impl = new implementation();
    }
    return impl;
}
} /* ns */
