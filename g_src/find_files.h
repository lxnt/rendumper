#if !defined(FIND_FILES_H)
#define FIND_FILES_H

/*
  parameters:
    pattern     - a glob pattern
    exception   - a file name to exclude from results

  return values:
    svector<char *>& - svector is an std::vector plus two methods, erase and insert
        that accept indices in lieu of iterators. So that's just a vector of char[],
        allocated by new inside the functions and freed by the called.

        TODO: get rid of variants that use this abomination.

    stringvectst& -
        A svector<pstringst *>, where pstringst itself is a struct with a single
        std::string member. Apparently an interface to text files.
*/

/* used in:
    class viewscreen_movieplayerst : viewscreenst       -- inteface.h/cpp
    list<string> enabler_inputst::list_macros()         -- enabler.cpp  */
void find_files_by_pattern(const char* pattern, svector<char *>& vec);

/* used in:
    libs/Dwarf_Fortress
    void graphicst::prepare_graphics(string &src_dir)   -- graphics.cpp */
void find_files_by_pattern_with_exception(const char* pattern, svector<char *>& vec, const char* exception);

/* used in:
    libs/Dwarf_Fortress                                                 */
void find_files_by_pattern(const char* pattern, stringvectst &vec);

/* used in: not used.
                                                                        */
void find_files_by_pattern_with_exception(const char* pattern, stringvectst &vec, const char* exception);

/* used in:
    libs/Dwarf_Fortress                                                 */
void find_directories_by_pattern(const char* pattern, stringvectst &filenames);

/* used in:
    libs/Dwarf_Fortress                                                 */
void find_directories_by_pattern_with_exception(const char* pattern, stringvectst &filenames, const char *exception);



#endif
