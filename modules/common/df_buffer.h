#if !defined(DF_BUFFER_H)
#define DF_BUFFER_H

#include "stdarg.h"
#include "itypes.h"

#define DFBUFOFFS(buf, wha) ((uint8_t *)NULL + ((uint8_t *)(buf->wha) - (buf->screen)))

void setup_buffer_t(df_buffer_t *buf, uint32_t pot);
df_buffer_t *new_buffer_t(uint32_t w, uint32_t h, uint32_t tail_sizeof);
df_buffer_t *allocate_buffer_t(uint32_t w, uint32_t h, uint32_t tail_sizeof = 0);
void dump_buffer_t(df_buffer_t *buf, const char *name);
void memset_buffer_t(df_buffer_t *, uint8_t);
void free_buffer_t(df_buffer_t *);
/*
    puts a string into the buffer. string gets truncated at size characters
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

/* push a string (for the ttf backend) */
void add_text_buffer_t(df_buffer_t *buffer, uint32_t x, uint32_t y, uint32_t len,
            uint32_t align, uint32_t wpix, uint32_t wgrid, const char *str, const char *attrs);

/* iterate over strings - set *state to NULL. After each call *state points to the current df_string_t,
   *attrs points to its attrs, pointer to character data (guaranteed to be zero-terminated) is returned.
   Return of NULL means the end. */
const char *iter_text_buffer_t(df_buffer_t *buffer, df_string_t **state, uint8_t **attrs);

/* pretend there's no strings */
void reset_text_buffer_t(df_buffer_t *buffer);

#endif
