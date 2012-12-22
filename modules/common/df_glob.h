#if !defined(DF_GLOB_H)
#define DF_GLOB_H

const char * const *df_glob(const char* pattern, const char * const exclude[],
                    const bool include_dirs, const bool include_files);

void df_gfree(const char * const * rv);

#endif
