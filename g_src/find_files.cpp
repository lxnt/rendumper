#include "svector.h"
#include "stringvecst.h"

#include "iplatform.h"

void find_files_by_pattern(const char* pattern, svector<char *>& filenames) {
    const char * const *glob = getplatform()->glob(pattern, NULL, false, true);
    const char * const *whoa = glob;

    while (*whoa) {
        char *c = new char[strlen(*whoa) + 1];
        strcpy(c, *whoa);
        filenames.push_back(c);
        whoa++;
    }
    getplatform()->gfree(glob);
}

void find_files_by_pattern_with_exception(const char* pattern, svector<char *>& filenames, const char *exception) {
    const char * const exclude[2] = { exception, NULL };
    const char * const *glob = getplatform()->glob(pattern, exclude, false, true);
    const char * const *whoa = glob;

    while (*whoa) {
        char *c = new char[strlen(*whoa) + 1];
        strcpy(c, *whoa);
        filenames.push_back(c);
        whoa++;
    }
    getplatform()->gfree(glob);
}

void find_files_by_pattern(const char* pattern, stringvectst &filenames) {
    const char * const *glob = getplatform()->glob(pattern, NULL, false, true);
    const char * const *whoa = glob;

    while (*whoa)
        filenames.add_string(*(whoa++));

    getplatform()->gfree(glob);
}

void find_files_by_pattern_with_exception(const char* pattern, stringvectst &filenames, const char *exception) {
    const char * const exclude[2] = { exception, NULL };
    const char * const *glob = getplatform()->glob(pattern, exclude, false, true);
    const char * const *whoa = glob;

    while (*whoa)
        filenames.add_string(*(whoa++));

    getplatform()->gfree(glob);
}

void find_directories_by_pattern(const char* pattern, stringvectst &filenames) {
    const char * const *glob = getplatform()->glob(pattern, NULL, true, false);
    const char * const *whoa = glob;

    while (*whoa)
        filenames.add_string(*(whoa++));

    getplatform()->gfree(glob);
}

void find_directories_by_pattern_with_exception(const char* pattern, stringvectst &filenames, const char *exception) {
    const char * const exclude[2] = { exception, NULL };
    const char * const *glob = getplatform()->glob(pattern, exclude, true, false);
    const char * const *whoa = glob;

    while (*whoa)
        filenames.add_string(*(whoa++));

    getplatform()->gfree(glob);
}
