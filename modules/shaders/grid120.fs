#version 120
#line 2 0

uniform sampler2D font;
uniform sampler3D findex;
uniform float final_alpha;
uniform vec3 pszar;             // { Parx, Pary, Psz }
uniform vec2 grid_wh;
uniform vec4 fofindex_wh;       // texture sizes
uniform vec4 colors[16];

varying vec4 ansicolors;        // tile: computed foreground and background color indices for tile and creature
varying vec4 tilecrea;         	// floor and creature tile indexes

vec4 idx2texco(float idx, out vec2 magray) {
    vec3 fi_pos;
    vec4 fo_pos;
    vec4 fo_szf;

    magray = vec2(0,0);

    if (idx < 0)                // no creature
        return vec4(0,0,0,0);

    /* findex texture holds

        struct df_taindex_entry_t {
            SDL_Rect rect;
            bool magentic;
            bool gray;
        };

        that's 2x uint16 + 2x uint8  + 2x bool.

        how do we do this in GLSL 1.20?  3D texture. In fact 2DA would fit better, but.
        internal: RGBA.
        layers:
            0 pos.x,msb, pos.x,lsb, pos.y,msb, pos.y,lsb
            1 celw, celh, magentic, gray
        all values in texels.
    */

    fi_pos.x = fract(idx / fofindex_wh.z);
    fi_pos.y = floor(idx / fofindex_wh.z) / fofindex_wh.w;

    fi_pos.z = 0.25;
    fo_pos = texture3D(findex, fi_pos);

    fi_pos.z = 0.75;
    fo_szf = texture3D(findex, fi_pos);

    vec4 cel_tc;
    cel_tc.x = ( 256.0 * fo_pos.x * 255.0 + 255.0 * fo_pos.y ) / fofindex_wh.x; // cel texture coords,
    cel_tc.y = ( 256.0 * fo_pos.z * 255.0 + 255.0 * fo_pos.w ) / fofindex_wh.y; // normalized
    cel_tc.z = 255.0 * fo_szf.x / fofindex_wh.x;  // cel texture size
    cel_tc.w = 255.0 * fo_szf.y / fofindex_wh.y;  // normalized to... texcoords?
    
    magray = 2*fo_szf.zw - 1;

    return cel_tc;
}

vec4 mg_process(vec4 color, vec2 magray) {
    if (    (magray.x > 0)
         && (color.r > 0.99)
         && (color.g < 0.01)
         && (color.b > 0.99))
                return vec4(0,0,0,0);

    if (magray.y > 0) // df orig uses 0.3/0.59/0.11
        return vec4(0.229 * color.r, 0.587 * color.g, 0.114 * color.b, color.a);

    return color;
}

void main() {
    vec2 pc = gl_PointCoord/pszar.xy;
    if ((pc.x > 1.0) || (pc.y > 1.0)) {
        discard;
    }

    vec2 tile_mg;
    vec2 crea_mg;

    vec4 tile = idx2texco(tilecrea.x, tile_mg);
    vec4 creature = idx2texco(tilecrea.y, crea_mg);

    vec4 texcoords = vec4 (tile.x + pc.x*tile.z,
                           tile.y + pc.y*tile.w,
                           creature.x + pc.x*creature.z,
                           creature.y + pc.y*creature.w);

    vec4 tile_color = mg_process(texture2D(font, texcoords.xy), tile_mg);
    vec4 crea_color = mg_process(texture2D(font, texcoords.zw), crea_mg);
    vec4 fg_color = colors[int(ansicolors.x)];
    vec4 bg_color = colors[int(ansicolors.y)];
    vec4 cf_color = colors[int(ansicolors.z)];
    vec4 cb_color = colors[int(ansicolors.w)];

    if (tilecrea.y > 0) // no creatures with idx==0 atm.
        gl_FragColor = mix(crea_color*cf_color, cb_color, 1.0 - crea_color.a);
    else
        gl_FragColor = mix(tile_color*fg_color, bg_color, 1.0 - tile_color.a);

    gl_FragColor.a = final_alpha;
}
