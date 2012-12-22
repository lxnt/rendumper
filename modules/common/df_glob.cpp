#include <stdlib.h>
#include <string.h>

#if defined(__WIN32__) || defined(__CYGWIN__)

#include <windows.h>

const char * const *df_glob(const char* pattern, const char * const exclude[],
                    const bool include_dirs, const bool include_files) {

    size_t allocd = sizeof(char *) * 1024;
    size_t used = 0;
    char **rv = (char **) calloc(allocd, sizeof(char *));

    HANDLE h;
    WIN32_FIND_DATA finddata;

    h = FindFirstFile(pattern, &finddata);
    
    if (h != INVALID_HANDLE_VALUE)
        do {
            if ((strcmp(finddata.cFileName, ".") == 0) || (strcmp(finddata.cFileName, "..") == 0))
                continue;

            if (finddata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (!include_dirs)
                    continue;
            } else {
                if (!include_files)
                    continue;
            }

            char *basename = strrchr(finddata.cFileName, '\\');
            if (!basename)
                basename = finddata.cFileName;
            else
                basename += 1;

            bool excluded = false;
            if (exclude)
                for (const char * const *e = exclude; *e != NULL; e++)
                    if (!strcmp(basename, *e)) {
                        excluded = true;
                        break;
                    }
            if (not excluded)
                rv[used++] = strdup(basename);
        } while (FindNextFile(h, &finddata));

    return rv;
}

#else

#include <glob.h>
#include <sys/stat.h>

const char * const *df_glob(const char* pattern, const char * const exclude[],
                    const bool include_dirs, const bool include_files) {

    glob_t g;
    size_t allocd = sizeof(char *) * 1024;
    size_t used = 0;
    char **rv = (char **) calloc(allocd, sizeof(char *));

    if (!::glob(pattern, 0, NULL, &g))
        for (size_t i = 0; i < g.gl_pathc; i++) {
            if (used == allocd - 1) {
                char **xv = (char **) calloc(2*allocd, sizeof(char *));
                if (rv) {
                    memmove(xv, rv, sizeof(char *) * allocd);
                    free(rv);
                }
                rv = xv;
            }

            struct stat cstat;
            stat(g.gl_pathv[i], &cstat);

            if (S_ISREG(cstat.st_mode) && !include_files)
                continue;

            if (S_ISDIR(cstat.st_mode) && !include_dirs)
                continue;

            char *basename = strrchr(g.gl_pathv[i], '/');
            if (!basename)
                basename = g.gl_pathv[i];
            else
                basename += 1;

            bool excluded = false;
            if (exclude)
                for (const char * const *e = exclude; *e != NULL; e++)
                    if (!strcmp(basename, *e)) {
                        excluded = true;
                        break;
                    }
            if (not excluded)
                rv[used++] = strdup(basename);
        }
    globfree(&g);
    return rv;
}

#endif

void df_gfree(const char * const * rv) {
    const char * const *whoa = rv;
    while (*whoa)
        free((void *) *(whoa++));
    free((void *)rv);
}
