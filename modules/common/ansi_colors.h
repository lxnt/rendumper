#if !defined(ANSICOLORS_H)
#define ANSICOLORS_H

typedef int ansi_colors_t[16][3];

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
