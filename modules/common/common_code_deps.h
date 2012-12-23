#if !defined(COMMON_CODE_DEPS_H)
#define COMMON_CODE_DEPS_H

#include "iplatform.h"
#include "imqueue.h"
#include "irenderer.h"

/* dependencies */
extern iplatform *platform;
extern imqueue *mqueue;
extern irenderer *renderer;
extern getrenderer_t _getrenderer;
void _get_deps(void);

#endif
