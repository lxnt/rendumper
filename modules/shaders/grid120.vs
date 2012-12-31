#version 120
#line 2 0

const float ANSI_CC = 16.0; // ansi color count

uniform vec4 txsz;              // { w_tiles, h_tiles, tile_w, tile_h }
uniform vec3 pszar; 			// { parx, pary, psz }
uniform vec2 grid_wh;
// Total 9 float uniforms

attribute vec4 screen;          // { ch, fg, bg, bold }
attribute float texpos;         //  tile_tex_idx
attribute float addcolor;
attribute float grayscale;
attribute float cf;
attribute float cbr;
attribute float fx;
attribute float vertex_id;
// Total 8 attributes  = 11 float values

varying vec4 ansicolors;        // computed foreground and background color indexes for tile and creature
varying vec4 tilecrea;         	// floor and creature tile indices, effects
// Total 6 float varyings (8 on Mesa)

vec2 ansiconvert(vec3 c) { // { fg, bg, bold }, returns {fg_idx, bg_idx}
    vec2 rv;
    float bold_factor = 0.0;
    if (c.z > 0.1)
        bold_factor = 8.0;

    rv.x = mod(c.x + bold_factor, ANSI_CC);
    rv.y = mod(c.y, ANSI_CC);
    return rv;
}

void main() {
    vec2 grid;
    grid.x = floor(vertex_id / grid_wh.y);
    grid.y = grid_wh.y - mod(vertex_id, grid_wh.y) - 1.0;

    vec2 posn = 2.0 * (grid.xy + 0.5)/grid_wh - 1.0;
    gl_Position = vec4(posn.x, posn.y, 0.0, 1.0);
    gl_PointSize = pszar.z;

    ansicolors.xy = ansiconvert(screen.yzw);
    ansicolors.zw = vec2(15.0, 0.0);
    tilecrea = vec4(screen.x, texpos, fx * 255.0, 0);

    if (texpos > 0.1) {
        if  (grayscale > 0.1)
            ansicolors.zw = ansiconvert(vec3(cf, screen.z, cbr));
        else if (addcolor > 0.1)
            ansicolors.zw = ansicolors.xy;
    } else {
        tilecrea.y = -42; // no creature.
    }
}
