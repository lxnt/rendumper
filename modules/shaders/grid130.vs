#version 130
#line 2 0

const int ANSI_CC = 16;      // ansi color count

uniform ivec4 txsz;          // { w_tiles, h_tiles, tile_w, tile_h }
uniform vec3  pszar;         // { parx, pary, psz }
uniform ivec2 grid_wh;       //   grid size

attribute ivec4 screen;      // { ch, fg, bg, bold }
attribute int   texpos;      //   tile_tex_idx
attribute int   addcolor;
attribute int   grayscale;
attribute int   cf;
attribute int   cbr;
attribute int   fx;

flat out ivec4 ansicolors;   // computed foreground and background color indices for tile and creature
flat out ivec4 tilecrea;     // floor and creature tile indices, effects

ivec2 ansiconvert(ivec3 c) { // { fg, bg, bold }, returns { fg_idx, bg_idx }
    int bold_factor = 0;
    if (c.z > 0)
        bold_factor = 8;

    return ivec2(mod(c.x + bold_factor, ANSI_CC), mod(c.y, ANSI_CC));
}

void main() {
    ivec2 grid;
    grid.x = gl_VertexID / grid_wh.y;
    grid.y = grid_wh.y - gl_VertexID % grid_wh.y - 1;

    vec2 posn = 2.0 * (grid.xy + 0.5)/grid_wh - 1.0;
    gl_Position = vec4(posn.x, posn.y, 0.0, 1.0);
    gl_PointSize = pszar.z;

    ansicolors.xy = ansiconvert(screen.yzw);
    ansicolors.zw = ivec2(15, 0);
    tilecrea = ivec4(screen.x, texpos, fx, 0);

    if (texpos > 0) {
        if  (grayscale > 0)
            ansicolors.zw = ansiconvert(ivec3(cf, screen.z, cbr));
        else if (addcolor > 0)
            ansicolors.zw = ansicolors.xy;
    } else {
        tilecrea.y = -42; // no creature.
    }
}
