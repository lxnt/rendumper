//#include <stdsomething.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include "itypes.h"
#include "iplatform.h"

/*  malloc(3) says:
        The malloc() and calloc() functions return a pointer
        to the allocated memory that is suitably aligned for
        any  kind  of variable.

    memalign(3) says:
        The  glibc  malloc(3) always returns 8-byte aligned
        memory addresses, so these functions are only needed
        if you require larger alignment values. */

static inline uint32_t pot_align(uint32_t value, uint32_t pot) {
    return (value + (1<<pot) - 1) & ~((1<<pot) -1);
}

static inline void *pot_align(void *ptr, uint32_t pot) {
    uint64_t value = (uint64_t)ptr;
    return (void *)((value + (1<<pot) - 1) & ~((1<<pot) -1));
}


/*  If we're streaming data to OpenGL, we should do glMapBuffer(), set up
    the buffer pointers and give the buffer to the simuloop for filling up with data.
    When buffer gets back to the renderer, it just does glBindBuffer, glUnmapBuffer,
    glBindVertexArray, glDrawArrays and that's it. Same stuff as in fgt.gl.SurfBunchPBO,
    but for vertices instead of textures.

    Since buffer object winds up in VAO state, the renderer should have at least two VAOs
    with corresponding glBuffers and df_buffer_t-s.

    Now, how can we be sure nothing reads from there so we can use GL_WRITE for access?
    No way, we just resort to GL_READ_WRITE. In any case we already save one memmove
    and let GL implementation do the alignment and actual allocation.

    A cursory glance reveals graphicst::dim_colors() which does reads. This might be
    possible to shift to shader, writing only positions to 'dim' to the tail.

*/

/* if buf->ptr is null, writes required allocation size to buf->required_sz.
   if buf->ptr is not null, sets up the pointers.
   buf->w, buf->h and buf->tail_sizeof must be valid.
*/

void setup_buffer_t(df_buffer_t *buf, uint32_t pot) {
    const uint32_t page_sz      = 4096;
    const uint32_t screen_sz    = pot_align(buf->w*buf->h*4, pot);
    const uint32_t texpos_sz    = pot_align(buf->w*buf->h*sizeof(long), pot);
    const uint32_t addcolor_sz  = pot_align(buf->w*buf->h, pot);
    const uint32_t grayscale_sz = pot_align(buf->w*buf->h, pot);
    const uint32_t cf_sz        = pot_align(buf->w*buf->h, pot);
    const uint32_t cbr_sz       = pot_align(buf->w*buf->h, pot);
    const uint32_t fx_sz        = pot_align(buf->w*buf->h, pot);
    const uint32_t tail_sz      = pot_align(buf->w*buf->h*buf->tail_sizeof, pot);

    buf->required_sz = screen_sz + texpos_sz + addcolor_sz
            + grayscale_sz + cf_sz + cbr_sz + fx_sz + tail_sz + page_sz;

    if (!buf->ptr) { // nominal; set up just required_sz.
        buf->screen = NULL;
        buf->texpos = NULL;
        buf->addcolor = NULL;
        buf->cf = NULL;
        buf->cbr = NULL;
        buf->fx = NULL;
        buf->tail = NULL;
        buf->used_sz = 0;
        return;
    }

    buf->screen = (unsigned char *) pot_align(buf->ptr, pot);
    buf->texpos = (long *) (buf->screen + screen_sz);
    buf->addcolor = (char *) (buf->screen + screen_sz + texpos_sz);
    buf->grayscale = buf->screen + screen_sz + texpos_sz + addcolor_sz;
    buf->cf = buf->screen + screen_sz + texpos_sz + addcolor_sz + grayscale_sz;
    buf->cbr = buf->screen + screen_sz + texpos_sz + addcolor_sz + grayscale_sz + cf_sz;
    buf->fx = buf->screen + screen_sz + texpos_sz + addcolor_sz + grayscale_sz + cf_sz + cbr_sz;
    buf->tail = buf->screen + screen_sz + texpos_sz + addcolor_sz + grayscale_sz + cf_sz + cbr_sz + fx_sz;

    buf->used_sz = buf->tail - buf->screen + tail_sz;
}

/* a constructor */
df_buffer_t *new_buffer_t(uint32_t w, uint32_t h, uint32_t tail_sizeof) {
    df_buffer_t *buf = (df_buffer_t *) malloc(sizeof(df_buffer_t));
    buf->pstate = 0;
    buf->ptr = NULL;
    buf->tail_sizeof = tail_sizeof;
    buf->w = w, buf->h = h;
    return buf;
}

/* whip up a malloc-backed 64bit-aligned buffer */
df_buffer_t *allocate_buffer_t(uint32_t w, uint32_t h, uint32_t tail_sizeof) {
    df_buffer_t *buf = new_buffer_t(w, h, tail_sizeof);
    setup_buffer_t(buf, 3);
    buf->ptr = (uint8_t *) malloc(buf->required_sz);
    setup_buffer_t(buf, 3);
    return buf;
}

void memset_buffer_t(df_buffer_t *buf, uint8_t b) {
    memset(buf->screen, b, buf->used_sz - buf->w*buf->h*buf->tail_sizeof);
}

void dump_buffer_t(df_buffer_t *buf, const char *name) {
    FILE *fp = fopen(name, "wb");
    fprintf(fp, "w=%u\nh=%u\ntail_sizeof=%u\nrequired_sz=%u\nused_sz=%u\nptr=%p\n",
        buf->w, buf->h, buf->tail_sizeof, buf->required_sz, buf->used_sz, buf->ptr);

    unsigned bc = 0;
    uint8_t *p = buf->ptr;
    while(bc < buf->required_sz) {
        bc++;
        fprintf(fp, "%02x ", (unsigned)p[bc]);
        if (bc%4 == 0)
            fputc(' ', fp);
        if (bc%32 == 0)
            fputc('\n', fp);
        if (p + bc == buf->screen)
            fputs("\nscreen:\n", fp);
        if (p + bc == (uint8_t *)buf->texpos)
            fputs("\ntexpos:\n", fp);
        if (p + bc == (uint8_t *)buf->addcolor)
            fputs("\naddcolor:\n", fp);
        if (p + bc == buf->grayscale)
            fputs("\ngrayscale:\n", fp);
        if (p + bc == buf->cf)
            fputs("\ncf:\n", fp);
        if (p + bc == buf->cbr)
            fputs("\ncbr:\n", fp);
        if (p + bc == buf->tail)
            fputs("\ntail:\n", fp);
        if (p + bc == buf->tail + buf->w*buf->h*buf->tail_sizeof)
            fputs("\nunused:\n", fp);

    }
    fputc('\n', fp);
    fclose(fp);
}

/*  if using external memory allocator, set buf->ptr to NULL
    after releasing its memory. */
void free_buffer_t(df_buffer_t *buf) {
    if (buf->ptr)
        free(buf->ptr);
    free(buf);
}

#if defined(__WIN32__) || defined(__CYGWIN__)
/*  There's some incompehensible stuff about linking
    with msvcrt, so just skip this for now */
#if 0

#include "crtdbg.h"
/* http://msdn.microsoft.com/en-us/library/a9yf33zb%28v=vs.80%29.aspx */
static void iph_stub(const wchar_t * /* expr */,
              const wchar_t * /* function */,
              const wchar_t * /* file */,
              unsigned int    /* line */,
              uintptr_t) { }

static void windoze_crap() {
    _set_invalid_parameter_handler(iph_stub);

/*  work around #define _CrtSetReportMode(t,f) ((int)0)
    causing unused value warning */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
    _CrtSetReportMode(_CRT_ASSERT, 0);
#pragma GCC diagnostic pop
}
#endif

#define vsnprintf _vsnprintf
#endif

int vbprintf(df_buffer_t *buffer, uint32_t x, uint32_t y, size_t size, uint32_t attrs, const char *fmt, va_list ap) {
    char tbuf[size + 1];

    memset(tbuf, 0, size+1);

#if defined(WIN32) && defined(MSVCRT_LINKING_GOT_COMPREHENDED)
    windoze_crap();
#endif

    int rv = vsnprintf(tbuf, size + 1, fmt, ap);

#if defined(WIN32)
    if (rv < 0) {
        if (strlen(tbuf) == size)
            rv = size;
        else
            return rv;
    }
#else
    if (rv < 0)
        return rv;
#endif
    uint32_t printed = rv;
    uint32_t i;
    uint32_t *ptr = (uint32_t *)(buffer->screen);

    for (i = 0 ; (i < buffer->w - x) && ( i < printed ); i++) {
        uint32_t offset = (x + i) * buffer->h + y;
        *( ptr + offset ) = attrs | tbuf[i];
    }
    return i;
}
