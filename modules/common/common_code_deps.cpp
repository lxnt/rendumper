#include "iplatform.h"
#include "imqueue.h"
#include "irenderer.h"
#include "itextures.h"
#include "isimuloop.h"
#include "imusicsound.h"
#include "ikeyboard.h"

#define DFMODULE_BUILD
#include "ideclspec.h"

/* dependencies */
iplatform *platform = NULL;
imqueue *mqueue = NULL;
irenderer *renderer = NULL;

extern "C" DECLSPEC void APIENTRY dependencies(iplatform ***pl, imqueue ***mq, irenderer ***rr, itextures ***tx, isimuloop ***sl, imusicsound ***ms, ikeyboard ***kb) {
    *pl = &platform;
    *mq = &mqueue;
    *rr = &renderer;
    *tx = NULL;
    *sl = NULL;
    *ms = NULL;
    *kb = NULL;
}