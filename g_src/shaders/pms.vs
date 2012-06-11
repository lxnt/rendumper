#version 130

uniform vec3 ansicolors[16];

uniform vec3 pszar; 		// { parx, pary, psz }
uniform ivec2 gridsize;

in ivec4 screen;         // { ch, fg, bg, bold } 
in  int  texpos;         //  tile_tex_idx 
in  int  addcolor;
in  int  grayscale;
in  int  cf;
in  int  cbr;
in ivec2 position;

flat out  int  ch;
flat out  vec3 fg;
flat out  vec3 bg;

void ansilookup(in ivec3 c, out vec3 fg, out vec3 bg) { // { afg, abg, bold }, returns fg, bg
    if (c.z > 0)
        fg = ansicolors[(c.x + 8) % 16];
    else
        fg = ansicolors[c.x % 16];
    bg = ansicolors[c.y % 16];
}

void main() {
    vec2 posn = 2.0 * (vec2(position.x,  gridsize.y -  position.y - 1) + 0.5)/gridsize - 1.0;
    gl_Position = vec4(posn.x, posn.y, 0.0, 1.0);
    gl_PointSize = pszar.z;
    
    if (texpos > 0) {
        if (grayscale > 0) {
            ansilookup(ivec3(cf, screen.z, cbr), fg, bg);
        } else if (addcolor > 0) {
            ansilookup(screen.yzw, fg, bg);
            ch = screen.x;
        }
        ch = texpos;
    } else {
        ansilookup(screen.yzw, fg, bg);
        ch = screen.x;
    }   
}
