#include <cstdio>
//#include <cstdarg>

#include "uthash.h"

#include "settings.h"
#include "iplatform.h"
#include "ansi_colors.h"

static struct cc_setting {
    char *key;
    char *value;

    UT_hash_handle hh;
} *settings = NULL;

/* piggyback on the logging lock for now; settings aren't performance-critical at all.
   just don't use this after initialization/setup is done.
   also, no cleanup is done.
*/

void cc_set_setting(const char* filename, const char *name, const char *value) {
    iplatform *slab = getplatform();
    ilogger *logr = slab->getlogr("df.settings");
    logr->info("cc_set_setting(%s.%s = '%s')", filename, name, value);
    cc_setting *item = NULL;
    slab->lock_logging();
    HASH_FIND(hh, settings, name, strlen(name), item);
    if (!item) {
        item = (cc_setting *)malloc(sizeof(cc_setting));
        item->key = (char *)malloc(strlen(filename) + strlen(name) + 2);
        item->value = (char *)malloc(strlen(value)+1);
        memcpy(item->value, value, strlen(value)+1);
        sprintf(item->key, "%s.%s", filename, name);
        HASH_ADD_KEYPTR(hh, settings, item->key, strlen(item->key), item);
        logr->trace("hash-added '%s' -> '%s'; head=%p", item->key, item->value, (void*)settings); // will deadlock.
    } else {
        item->value = (char *)realloc(item->value, strlen(value)+1);
        memcpy(item->value, value, strlen(value)+1);
    }
    slab->unlock_logging();
}

const char *cc_get_setting(const char *name, const char *fallback) {
    iplatform *slab = getplatform();
    ilogger *logr = slab->getlogr("df.settings");
    cc_setting *item = NULL;
    const char *rv = fallback;
    slab->lock_logging();
    HASH_FIND(hh, settings, name, strlen(name), item);
    if (item)
        rv = item->value;
    else {
        logr->info("cc_get_setting(%s) head=%p", name, (void *)settings);
        cc_setting *tmp, *s;
        HASH_ITER(hh, settings, s, tmp) {
            logr->info("cc_get_setting(%s) [dump] ='%s'", s->key, s->value);
        }
    }
    slab->unlock_logging();
    logr->info("cc_get_setting(%s): ='%s'", name, rv);
    return rv;
}

static ansi_colors_t ansi_colors_vga = ANSI_COLORS_VGA;
static ansi_colors_t ansi_colors_xterm = ANSI_COLORS_XTERM;
static ansi_colors_t ansi_colors_xp = ANSI_COLORS_XP;
static ansi_colors_t ansi_colors_txt = ANSI_COLORS_XP;

static const int ansi_to_df[16] = ANSI_TO_DF;

static ansi_colors_t *convert_color_order(ansi_colors_t *src, ansi_colors_t *dst) {
    for (int i=0; i < 16; i++) {
        (*dst)[i][0] = (*src)[ansi_to_df[i]][0];
        (*dst)[i][1] = (*src)[ansi_to_df[i]][1];
        (*dst)[i][2] = (*src)[ansi_to_df[i]][2];
    }
    return dst;
}

const ansi_colors_t *cc_get_colors(void) {
    const char * const csname = cc_get_setting("init.COLORSET", NULL);
    ilogger *logr = getplatform()->getlogr("df.settings.colors");
    if (csname) {
        if (0 == strcmp(csname, "VGA"))
            return convert_color_order(&ansi_colors_vga, &ansi_colors_txt);
        if (0 == strcmp(csname, "XTERM"))
            return convert_color_order(&ansi_colors_xterm, &ansi_colors_txt);
        if (0 == strcmp(csname, "XP"))
            return convert_color_order(&ansi_colors_xp, &ansi_colors_txt);
        logr->fatal("cc_get_colors(): unknown colorset '%s'", csname);
    }
    /* this is in the DF order - as in enabler.ccolor (see init.cpp)
       df attrs are direct indices into that. */
    const char *ccnames[16][3] =  {
        { "BLACK_R",    "BLACK_G",    "BLACK_B",   },
        { "BLUE_R",     "BLUE_G",     "BLUE_B",    },
        { "GREEN_R",    "GREEN_G",    "GREEN_B",   },
        { "CYAN_R",     "CYAN_G",     "CYAN_B",    },
        { "RED_R",      "RED_G",      "RED_B",     },
        { "MAGENTA_R",  "MAGENTA_G",  "MAGENTA_B", },
        { "BROWN_R",    "BROWN_G",    "BROWN_B",   },
        { "LGRAY_R",    "LGRAY_G",    "LGRAY_B",   },
        { "DGRAY_R",    "DGRAY_G",    "DGRAY_B",   },
        { "LBLUE_R",    "LBLUE_G",    "LBLUE_B",   },
        { "LGREEN_R",   "LGREEN_G",   "LGREEN_B",  },
        { "LCYAN_R",    "LCYAN_G",    "LCYAN_B",   },
        { "LRED_R",     "LRED_G",     "LRED_B",    },
        { "LMAGENTA_R", "LMAGENTA_G", "LMAGENTA_B",},
        { "YELLOW_R",   "YELLOW_G",   "YELLOW_B",  },
        { "WHITE_R",    "WHITE_G",    "WHITE_B"    }
    };
    char ccsname[1024];
    for (int i=0; i < 16; i++)
        for (int j=0; j < 3; j++) {
            sprintf(ccsname, "colors.%s", ccnames[i][j]);
            const char *ccval = cc_get_setting(ccsname, NULL);
            if (!ccval)
                logr->fatal("cc_get_colors(): colors.txt doesn't have '%s'", ccsname);
            ansi_colors_txt[i][j] = atoi(ccval);
            logr->trace("cc_get_colors(): '%s' -> %d", ccsname, ansi_colors_txt[i][j]);
        }
    return &ansi_colors_txt;
}
