#if !defined(POSIX_GLOB_H)
#define POSIX_GLOB_H

const char * const *posix_glob(const char* pattern, const char * const exclude[],
                    const bool include_dirs, const bool include_files);

void posix_gfree(const char * const * rv);

#endif
