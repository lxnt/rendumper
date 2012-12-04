#if defined(DECLSPEC)
# undef DECLSPEC
#endif
#if defined(__WIN32__)
# if defined(DFMODULE_BUILD)
#  define DECLSPEC __declspec(dllexport)
# else
#  define DECLSPEC __declspec(dllimport)
# endif
# define APIENTRY __stdcall
#else
# define DECLSPEC
# define APIENTRY
#endif