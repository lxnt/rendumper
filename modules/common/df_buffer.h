#if !defined(DF_BUFFER_H)
#define DF_BUFFER_H

#include "stdarg.h"
#include "itypes.h"

/* offset macro for glBufferData() */
#define DFBUFOFFS(buf, wha) ((uint8_t *)NULL + ((uint8_t *)(buf->wha) - (buf->screen)))

/* constructors.

    new_buffer_t()      - backing store (.ptr) is user-managed (glMapBuffer, etc).
    allocate_buffer_r() - backing store is on the heap and realloc_buffer_t() can be used

    w, h - grid size
    tail_sizeof - per-grid-cell aux data type size
    pot - screen (points to the first data unit intended for use) gets aligned to 2^pot bytes
            3 = 8 bytes; 6 = 64 bytes; 12 = 4096 bytes. */
df_buffer_t *new_buffer_t(uint32_t w, uint32_t h, uint32_t tail_sizeof, uint32_t pot);
df_buffer_t *allocate_buffer_t(uint32_t w, uint32_t h, uint32_t tail_sizeof, uint32_t pot);

/* return size of backing store needed for the buffer's current w and h*/
uint32_t size_buffer_t(df_buffer_t *buf);

/* set up pointers according to set alignment, tail_sizeof and current ptr,w,h values.*/
void setup_buffer_t(df_buffer_t *buf);

/* dump buffer data to a file */
void dump_buffer_t(df_buffer_t *buf, const char *name);

/* memset() all but the tail */
void memset_buffer_t(df_buffer_t *, uint8_t);

/* drop a buffer that had been had from a constructor. set buf->ptr to NULL if
   it was from new_buffer_t(). */
void free_buffer_t(df_buffer_t *);

/* resize a heap-backed buffer */
void realloc_buffer_t(df_buffer_t *buf, uint32_t w, uint32_t h, uint32_t tail_sizeof = 0);

/* put a string into the buffer. string gets truncated at size characters
    or at the buffer border; no wrapping.

    attr is a single byte:
        unBrBgBgBgFgFgFg

        i.e.:
            fg =  attr & 7
            bg = (attr >>3) & 7
            br = (attr >>6) & 1

        are handily printed as octal value : 0BrBgFg.

    return value:
        characters written, that is min(final_strlen, size, dimx-x)
        or -1.
*/
int vbprintf(df_buffer_t *buffer, uint32_t x, uint32_t y, uint32_t size, uint8_t attr, const char *fmt, va_list ap);

/* same as above, but without formatting and error-reporting */
unsigned bputs(df_buffer_t *buffer, uint32_t x, uint32_t y, uint32_t size, const char *str, uint8_t attr);

/* same as above, but with varying attrs */
unsigned bputs_attrs(df_buffer_t *buffer, uint32_t x, uint32_t y, uint32_t size, const char *str, const char *attrs);

#endif
