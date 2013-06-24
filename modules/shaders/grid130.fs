#version 130
#line 2 0

uniform sampler2D font;
uniform isampler2D findex;      // findex - a texture buffer
uniform float final_alpha;
uniform vec3 pszar;             // { Parx, Pary, Psz }
uniform vec4 colors[16];

flat in ivec4 ansicolors;       // tile: computed foreground and background color indices for tile and creature
flat in ivec4 tilecrea;         // floor and creature tile indices, effects

vec4 idx2texco(in int idx, out bvec2 magray) {
    magray = bvec2(false, false);

    if (idx < 0)                // no creature
        return vec4(0,0,0,0);

    ivec2 findex_wh = textureSize(findex, 0);
    ivec2 findex_xy = ivec2(idx % findex_wh.x, idx / findex_wh.x);
    ivec4 fdata = texelFetch(findex, findex_xy, 0);

    /* fdata.xy = cel indices into font; fdata.zw = cel size: w,h | magentic, gray */
    magray = bvec2( fdata.z < 0, fdata.w < 0 );

    vec4 rv;
    rv.xy = vec2(fdata.xy) / vec2(textureSize(font, 0)); // start-of-cel normalized coordinates
    rv.zw = vec2(abs(fdata.zw)) / vec2(textureSize(font, 0)); //normalized size-of-cel in .zw 
    return rv;
}

vec4 mg_process(vec4 color, bvec2 magray) {
    if (     magray.x
         && (color.r > 0.99)
         && (color.g < 0.01)
         && (color.b > 0.99))
                return vec4(0,0,0,0); // transparent black

    if (magray.y) // df orig uses 0.3/0.59/0.11
        return vec4(0.229 * color.r, 0.587 * color.g, 0.114 * color.b, color.a);

    return color;
}

void main() {
    vec2 pc = gl_PointCoord/pszar.xy;
    if ((pc.x > 1.0) || (pc.y > 1.0)) {
        discard;
    }

    bvec2 tile_mg;
    bvec2 crea_mg;

    vec4 tile = idx2texco(tilecrea.x, tile_mg);
    vec4 creature = idx2texco(tilecrea.y, crea_mg);

    vec4 texcoords = vec4 (tile.x + pc.x*tile.z,
                           tile.y + pc.y*tile.w,
                           creature.x + pc.x*creature.z,
                           creature.y + pc.y*creature.w);

    vec4 tile_color = mg_process(texture2D(font, texcoords.xy), tile_mg);
    vec4 crea_color = mg_process(texture2D(font, texcoords.zw), crea_mg);
    vec4 fg_color = colors[ansicolors.x];
    vec4 bg_color = colors[ansicolors.y];
    vec4 cf_color = colors[ansicolors.z];
    vec4 cb_color = colors[ansicolors.w];

    if (tilecrea.y > 0) // no creatures with idx==0 atm.
        gl_FragColor = mix(crea_color*cf_color, cb_color, 1.0 - crea_color.a);
    else
        gl_FragColor = mix(tile_color*fg_color, bg_color, 1.0 - tile_color.a);

    int dim = tilecrea.z & 7;
    int rain = tilecrea.z & 8;
    int snow = tilecrea.z & 16;

    if (dim > 0)
        gl_FragColor.a = final_alpha / (dim + 1);
    if (rain > 0)
        gl_FragColor.rgba = vec4(0.7, 0.7,1.0, 1.0);
    if (snow > 0)
        gl_FragColor.rgba = vec4(1.0,1.0,1.0, final_alpha);

}
