#version 130

uniform usampler2D      findex; // RGBA16UI: cs, cy, cw, ch
uniform sampler2D       font;

uniform vec3 pszar;
uniform float final_alpha;

flat in  int  ch;
flat in  vec3 fg;
flat in  vec3 bg;

out vec4 frag;

vec4 blit_execute(in vec2 pc, in int cindex, in vec4 fg, in vec4 bg) {
    ivec2 finsz = textureSize(findex,0);
    ivec2 fintc = ivec2(cindex % finsz.x, cindex / finsz.x);
    uvec4 cinfo = texelFetch(findex, fintc, 0);
    ivec2 fonsz = textureSize(font,0);
    
    // tile size in font texture normalized coordinates
    vec2 tilesizeN = vec2(float(cinfo.z)/float(fonsz.x), float(cinfo.w)/float(fonsz.y));
    // offset to the tile in font texture normalized coordinates
    vec2 offsetN = vec2(float(cinfo.x)/float(fonsz.x), float(cinfo.y)/float(fonsz.y));
    // finally, the texture coordinates for the fragment
    vec2 texcoords = offsetN + tilesizeN * pc;
    
    vec4 tile_color = textureLod(font, texcoords, 0);
    vec4 rv = mix(tile_color * fg, bg, 1.0 - tile_color.a);

    return rv;
}

void main() {
    vec2 pc = gl_PointCoord/pszar.xy;
    if ((pc.x > 1.0) || (pc.y > 1.0))
        discard;
    
    frag = blit_execute(pc, ch, vec4(fg,1), vec4(bg,1));
    frag.a = final_alpha;
}
