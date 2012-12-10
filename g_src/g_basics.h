#ifndef G_BASICS_H
# define G_BASICS_H
# define MAX_GRID_X 256
# define MAX_GRID_Y 256
# define MIN_GRID_X 80
# define MIN_GRID_Y 25
# ifndef MAX
#  define MAX(a, b) (((a) > (b)) ? (a) : (b))
# endif
# ifndef MIN
#  define MIN(a, b) (((a) < (b)) ? (a) : (b))
# endif
# ifndef ARRSZ
#  define ARRSZ(arr) (sizeof (arr) / sizeof ((arr)[0]))
# endif
# ifndef CLAMP
#  define CLAMP(x,a,b) MIN(MAX((x),(a)),(b))
# endif
#endif
