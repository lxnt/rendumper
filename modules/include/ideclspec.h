#if defined(DECLSPEC)
# undef DECLSPEC
#endif
#if defined(__WIN32__) || defined(__CYGWIN__)
# if defined(DFMODULE_BUILD)
#  define DECLSPEC __declspec(dllexport)
# else
#  define DECLSPEC __declspec(dllimport)
# endif
# define APIENTRY __stdcall
#else /* GCC 4+ is implied. -fvisibility=hidden is implied. */
# if defined(DFMODULE_BUILD) /* building modules */
#  define DECLSPEC __attribute__((visibility ("default")))
# else /* building glue or linking directly */
#  define DECLSPEC
# endif
# define APIENTRY
#endif
