//#include <stdsomething.h>
#include <cstdlib>
#include <cstring>
#include "itypes.h"

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
    const uint32_t tail_sz      = pot_align(buf->w*buf->h*buf->tail_sizeof, pot);

    buf->required_sz = screen_sz + texpos_sz + addcolor_sz
            + grayscale_sz + cf_sz + cbr_sz + tail_sz + page_sz;

    if (!buf->ptr)
        return;

    buf->screen = (unsigned char *) pot_align(buf->ptr, pot);
    buf->texpos = (long *) (buf->screen + screen_sz);
    buf->addcolor = (char *) (buf->screen + screen_sz + texpos_sz);
    buf->grayscale = buf->screen + screen_sz + texpos_sz + addcolor_sz;
    buf->cf = buf->screen + screen_sz + texpos_sz + addcolor_sz + grayscale_sz;
    buf->cbr = buf->screen + screen_sz + texpos_sz + addcolor_sz + grayscale_sz + cf_sz;
    buf->tail = buf->screen + screen_sz + texpos_sz + addcolor_sz + grayscale_sz + cf_sz + cbr_sz;

    buf->used_sz = buf->tail - buf->screen + tail_sz;
}

/* a constructor */
df_buffer_t *new_buffer_t(uint32_t w, uint32_t h, uint32_t tail_sizeof) {
    df_buffer_t *buf = (df_buffer_t *) malloc(sizeof(df_buffer_t));
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
    memset(buf->screen, b, buf->used_sz);
}

/*  if using external memory allocator, set buf->ptr to NULL
    after releasing its memory. */
void free_buffer_t(df_buffer_t *buf) {
    if (buf->ptr)
        free(buf->ptr);
    free(buf);
}
