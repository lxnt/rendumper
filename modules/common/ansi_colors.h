#if !defined(ANSICOLORS_H)
#define ANSICOLORS_H

/*
    ANSI/?GA palette color sequence is:

    black red green brown blue magenta cyan gray
    darkgray red green yellow blue magenta cyan white

    DF:

    black blue green cyan red magenta brown lgray
    dgray lblue lgreen lcyan lred lmagenta yellow white

    Stock DF colors.txt is the ANSI_COLORS_XP variant.

*/

/*
    ANSI black red  green brown blue magenta cyan  lgray dgray lred  lgreen yellow lblue lmagenta cyan   white
     DF  black blue green cyan  red  magenta brown lgray dgray lblue lgreen lcyan  lred  lmagenta yellow white
     M     *    x     *     x    x      *      x     *     *     x      *      x     x      *        x     *
*/

/* index mapping */
#define ANSI_TO_DF { \
    0,  4,  2,  6, 1,  5,  3,  7, \
    8, 12, 10, 14, 9, 13, 11, 15  }

#define ANSI_COLORS_VGA \
{                       \
    { 0, 0, 0 } ,       \
    { 170, 0, 0 },      \
    { 0, 170, 0 },      \
    { 170, 85, 0 },     \
    { 0, 0, 170 },      \
    { 170, 0, 170 },    \
    { 0, 170, 170 },    \
    { 170, 170, 170 },  \
                        \
    { 85, 85, 85 },     \
    { 255, 85, 85 },    \
    { 85, 255, 85 },    \
    { 255, 255, 85 },   \
    { 85, 85, 255 },    \
    { 255, 85, 255 },   \
    { 85, 255, 255 },   \
    { 255, 255, 255 },  \
}

#define ANSI_COLORS_XP  \
{                       \
    { 0, 0, 0 } ,       \
    { 128, 0, 0 },      \
    { 0, 128, 0 },      \
    { 128, 0, 0 },      \
    { 0, 0, 128 },      \
    { 128, 0, 128 },    \
    { 0, 128, 128 },    \
    { 192, 192, 192 },  \
                        \
    { 128, 128, 128 },  \
    { 255, 0, 0 },      \
    { 0, 255, 0 },      \
    { 255, 255, 0 },    \
    { 0, 0, 255 },      \
    { 255, 0, 255 },    \
    { 0, 255, 255 },    \
    { 255, 255, 255 },  \
}

#define ANSI_COLORS_XTERM \
{                       \
    { 0, 0, 0 } ,       \
    { 205, 0, 0 },      \
    { 0, 205, 0 },      \
    { 205, 0, 0 },      \
    { 0, 0, 205 },      \
    { 205, 0, 205 },    \
    { 0, 205, 205 },    \
    { 229, 229, 229 },  \
                        \
    { 127, 127, 127 },  \
    { 255, 0, 0 },      \
    { 0, 255, 0 },      \
    { 255, 255, 0 },    \
    { 92, 92, 255 },    \
    { 255, 0, 255 },    \
    { 0, 255, 255 },    \
    { 255, 255, 255 },  \
}

#endif
