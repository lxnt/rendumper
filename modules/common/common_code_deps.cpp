#include "itypes.h"

#if !defined(NULL)
#define NULL (0L)
#endif

/* dependencies */
iplatform *platform = NULL;
imqueue   *mqueue   = NULL;

/* dependency getter entry points */
getplatform_t _getplatform;
getmqueue_t   _getmqueue;
getrenderer_t _getrenderer;

/* helper for getwhatever()s */
void _get_deps(void) {
    if (!platform)
        platform = _getplatform();
    if (!mqueue)
        mqueue = _getmqueue();
}

/* linker entry point */
extern "C" DFM_EXPORT void DFM_APIEP dependencies(
                            getplatform_t **pl,
                            getmqueue_t **mq,
                            getrenderer_t **rr,
                            gettextures_t **tx,
                            getsimuloop_t **sl,
                            getmusicsound_t **ms,
                            getkeyboard_t **kb) {
    *pl = &_getplatform;
    *mq = &_getmqueue;
    *rr = &_getrenderer;
    *tx = NULL;
    *sl = NULL;
    *ms = NULL;
    *kb = NULL;
}
