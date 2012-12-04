#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "iplatform.h"

#include "glue.h"

int main (int argc, char* argv[]) {
    if (argc != 3) {
	fprintf(stderr, "Usage: %s module-path platform-module\n", argv[0]);
	return 1;
    }
    load_modules(argv[1], argv[2], NULL, NULL);

    iplatform *platform = getplatform();

    platform->MessageBox(NULL, "some alert text", "some alert caption", MB_YESNO);

    platform->release();
    return 0;
}
