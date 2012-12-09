#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "iplatform.h"

#include "glue.h"

int usage(const char *s) {
    getplatform()->log_error("Usage: %s module-path platform-name pattern {dirs|files|all} [exclude [exclude ...]]", s);
    return 1;
}

int main (int argc, char* argv[]) {
    if (argc < 5)
        return usage(argv[0]);

    load_platform(argv[2], argv[1]);

    iplatform *platform = getplatform();

    bool dirs = false, files = false;

    if (!strcmp("dirs", argv[4]))
        dirs = true;
    else if (!strcmp("files", argv[4]))
        files = true;
    else if (!strcmp("all", argv[4]))
        dirs = true, files = true;
    else
        return usage(argv[0]);

    const char *exclude[argc - 4];

    for(int i = 0; i < argc - 5; i++)
        exclude[i] = argv[i + 5];

    exclude[argc-5] = NULL;

    const char * const *glob = platform->glob(argv[3], exclude, dirs, files);
    const char * const *whoa = glob;

    while (*whoa) {
        platform->log_info("found  '%s'", *whoa);
        whoa++;
    }
    platform->gfree(glob);

    platform->release();
    return 0;
}
