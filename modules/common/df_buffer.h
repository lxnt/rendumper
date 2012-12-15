#if !defined(DF_BUFFER_H)
#define DF_BUFFER_H

#include "itypes.h"

#define DFBUFOFFS(buf, wha) ((char *)NULL + ((char *)(buf->##wha) - buf->screen)

void setup_buffer_t(df_buffer_t *buf, uint32_t pot);
df_buffer_t *new_buffer_t(uint32_t w, uint32_t h, uint32_t tail_sizeof);
df_buffer_t *allocate_buffer_t(uint32_t w, uint32_t h, uint32_t tail_sizeof = 0);
void memset_buffer_t(df_buffer_t *, uint8_t);
void free_buffer_t(df_buffer_t *);

#endif
