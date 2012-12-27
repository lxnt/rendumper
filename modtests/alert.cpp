#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "iplatform.h"

#include "glue.h"

int main (int argc, char* argv[]) {
    if (argc != 2) {
	fprintf(stderr, "Usage: %s platform-name\n", argv[0]);
	return 1;
    }
    if (!lock_and_load(argv[1], DF_MODULES_PATH, LL_TRACE))
        return 1;

    iplatform *platform = getplatform();

    platform->MessageBox(NULL, "some alert text", "some alert caption", MB_YESNO);

    platform->release();
    return 0;
}
