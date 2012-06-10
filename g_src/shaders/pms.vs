#version 130

uniform vec4 ansicolors[16];

uniform vec3 pszar; 		// { parx, pary, psz }
uniform ivec2 gridsize;

in ivec4 screen;         // { ch, fg, bg, bold } 
in uint  texpos;         //  tile_tex_idx 
in uint  addcolor;
in uint  grayscale;
in uint  cf;
in uint  cbr;
in ivec2 position;

flat out ivec4 tx;
flat out  vec4 fg;
flat out  vec4 bg;

void ansilookup(in ivec3 c, out vec4 fg, out vec4 bg) { // { afg, abg, bold }, returns fg, bg
    if (c.w > 0)
        fg = ansicolors[(c.y + 8) % 16];
    else
        fg = ansicolors[c.y % 16];
    bg = ansicolors[c.z % 16];
}

void main() {
    vec2 posn = 2.0 * (vec2(position.x, gridsize.y - position.y - 1) + 0.5)/gridsize - 1.0;
    gl_Position = vec4(posn.x, posn.y, 0.0, 1.0);
    gl_PointSize = pszar.z;
    
    if (texpos > 0) {
        if (grayscale > 0) {
            ansilookup(ivec3(cf, screen.z, cbr), fg, bg);
        } else if (addcolor > 0) {
            ansilookup(screen.yzw, fg, bg);
            tx.x = screen.x;
        }
        tx.x = screen.texpos;
    } else {
        ansilookup(screen.yzw, fg, bg);
        tx = screen.x;
    }  
}
