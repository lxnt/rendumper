#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "iplatform.h"

#include "glue.h"

int usage(const char *s) {
    if (getplatform)
        getplatform()->log_error("Usage: %s print-mode pattern {dirs|files|all} [exclude [exclude ...]]", s);
    else
        fprintf(stderr, "Usage: %s print-mode pattern {dirs|files|all} [exclude [exclude ...]]\n", s);
    return 1;
}

int main (int argc, char* argv[]) {
    if (argc < 3)
        return usage(argv[0]);

    if (!load_platform(argv[1], DF_MODULES_PATH))
        return usage(argv[0]);

    const char *pattern = argv[2];
    const char *mode = argv[3];

    iplatform *platform = getplatform();

    bool dirs = false, files = false;

    if (!strcmp("dirs", mode))
        dirs = true;
    else if (!strcmp("files", mode))
        files = true;
    else if (!strcmp("all", mode))
        dirs = true, files = true;
    else
        return usage(argv[0]);

    const char *exclude[argc - 3];

    for(int i = 0; i < argc - 4; i++)
        exclude[i] = argv[i + 4];

    exclude[argc - 4] = NULL;

    const char * const *glob = platform->glob(pattern, exclude, dirs, files);
    const char * const *whoa = glob;

    while (*whoa) {
        platform->log_info("found  '%s'", *whoa);
        whoa++;
    }
    platform->gfree(glob);

    platform->release();
    return 0;
}
