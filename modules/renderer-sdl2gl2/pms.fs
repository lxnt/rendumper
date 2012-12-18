#version 120
#line 2 0

uniform sampler2D ansi;
uniform sampler2D font;
uniform sampler2D findex;
uniform float final_alpha;
uniform vec3 pszar;             // { Parx, Pary, Psz }
uniform vec4 fofindex_wh;       // texture sizes

// Total 8 uniform floats, 3 samplers
varying vec4 ansicolors;        // tile: computed foreground and background color indices for tile and creature
varying vec2 tilecrea;         	// floor and creature tile indexes

// Total 6 float varyings (8 on Mesa)

vec4 idx2texco(float idx, out vec2 magray) {
    vec4 tile_size;
    vec2 fi_pos;
    vec4 fo_posz;

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

        how do we do this in GLSL 1.20?  GL_RGBA16 + denorm? 2DA or 3D texture?
        okay, let's try GL_RGBA16. rg would be texpos; ba - 15 bits size, two bits for bools.
    */

    fi_pos.x = fract(idx / fofindex_wh.z);
    fi_pos.y = floor(idx / fofindex_wh.z) / fofindex_wh.w;
    fo_posz = texture2D(findex, fi_pos);

    fo_posz.x = ( fo_posz.x * 16384 ) / fofindex_wh.x;
    fo_posz.y = ( fo_posz.y * 16384 ) / fofindex_wh.y;

    if (fo_posz.z > 0.5) {
        fo_posz.z = ( (fo_posz.z - 0.5) * 16384 ) / fofindex_wh.x;
        magray.x = 1;
    } else {
        fo_posz.z = ( (fo_posz.z ) * 16384 ) / fofindex_wh.x;
        magray.x = -1;
    }

    if (fo_posz.w > 0.5) {
        fo_posz.w = ( (fo_posz.w - 0.5) * 16384 ) / fofindex_wh.y;
        magray.y = 1;
    } else {
        fo_posz.w = ( (fo_posz.w ) * 16384 ) / fofindex_wh.y;
        magray.y = -1;
    }

    return fo_posz;
}

vec4 mg_process(vec4 color, vec2 magray) {
    if (    (magray.x > 0)
         && (color.r > 0.99)
         && (color.b < 0.01)
         && (color.g > 0.99))
                return vec4(0,0,0,0);

    if (magray.y > 0) // df orig uses 0.3/0.59/0.11
        return vec4(0.229 * color.r, 0.587 * color.g, 0.114 * color.b, color.a);

    return color;
}

void main() {
    vec2 pc = gl_PointCoord/pszar.xy;
    if ((pc.x > 1.0) || (pc.y >1.0)) {
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
#if 0
    vec4 crea_color = mg_process(texture2D(font, texcoords.zw), crea_mg);
    vec4 fg_color = texture2D(ansi, vec2(ansicolors.x, 0.0));
    vec4 bg_color = texture2D(ansi, vec2(ansicolors.y, 0.0));
    vec4 cf_color = texture2D(ansi, vec2(ansicolors.z, 0.0));
    vec4 cb_color = texture2D(ansi, vec2(ansicolors.w, 0.0));

    if (tilecrea.y > 0) // no creatures with idx==0 atm.
        gl_FragColor = mix(crea_color*cf_color, cb_color, 1.0 - crea_color.a);
    else
        gl_FragColor = mix(tile_color*fg_color, bg_color, 1.0 - tile_color.a);

#endif
    gl_FragColor = tile_color;
    gl_FragColor.a = final_alpha;
    //gl_FragColor = vec4(gl_PointCoord.s, gl_PointCoord.y, 0, 1);
}
