#if !defined (SETTINGS_H)
#define SETTINGS_H
#include "iplatform.h"

/* this to be used only in platform implementations */

const char *cc_get_setting(const char *name, const char *fallback);
void cc_set_setting(const char* filename, const char *name, const char *value);
const ansi_colors_t *cc_get_colors();

#endif
