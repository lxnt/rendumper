#define DFMODULE_BUILD
#include "iplatform.h"


copy_paste(platform-sdl20/mqueue.cpp)
reglue(mqueue_impl, pthreads)
