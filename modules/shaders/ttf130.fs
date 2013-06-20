#version 130
#line 2 0

const int ANSI_CC = 16;      // ansi color count
uniform vec4 colors[16];
uniform usampler2D text_tex; // GL_RG16I with rendered text/glyph indices
uniform vec2 text_size;      // its size, since we have it anyway
uniform usampler2D attr_tex; // GL_R8UI with attr data

in vec2 coord;
flat in ivec2 stuff;

/* attr data: FgBgBr for given cluster (texel.y or via clustermap) */

int attrfetch(int offset) {
    ivec2 attr_txc;
    ivec2 tsz = textureSize(attr_tex, 0);
    attr_txc.x = offset % tsz.x;
    attr_txc.y = offset / tsz.x;
    uvec4 attr = texelFetch(attr_tex, attr_txc, 0);
    return int(attr.x);
}

vec4 attrmix(int c, float alpha) {
    int bold_factor = (c & 64 )>>3;
    vec4 fg = colors[(c & 7) + bold_factor];
    vec4 bg = colors[(c & 56)>>3];
    return mix(fg, bg, 1.0 - alpha);
}

void main() {
    uvec4 txl = texture(text_tex, coord);
#if 1
    int fg_attr_offset = stuff.x;
    int clustermap_row = stuff.y;
    int cluster_index = int(txl.y);
#if 1
    if (txl.x == 0u) {
        /* we look up the cluster index 'just under' the rest
          of our string. denormalized texcoord x is just fine for that. */
        ivec2 attr_txc = ivec2(int(coord.x * text_size.x), clustermap_row);
        uvec4 clue = texelFetch(attr_tex, attr_txc, 0);
        cluster_index = int(clue.x);
    }
#endif
    int attr = attrfetch(fg_attr_offset + cluster_index);
    gl_FragColor = attrmix(attr, txl.x/65536.0);
#else
    gl_FragColor = vec4(txl.x/65536.0,txl.x/65536.0,txl.x/65536.0,1.0) ;

    //gl_FragColor = vec4(1.0,1.0,1.0,1.0) ;
    //gl_FragColor = vec4(coord.x,coord.y,0.0,1.0) ;
#endif
    gl_FragColor.a = 1.0;
}
