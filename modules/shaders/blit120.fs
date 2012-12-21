#version 120
#line 2 0

#define M_FILL   1  // fill with color
#define M_BLEND  2  // texture it
#define M_OPAQUE 3  // force texture alpha to 1.0
#define M_RALPHA 4  // fill with color; take alpha from tex

uniform int mode;
uniform sampler2D tex;
uniform vec4 color;

varying vec2 coord;

void main() {

    if (abs(mode - M_FILL) < 0.01) {
        gl_FragColor = color;
        return;
    }

    if (abs(mode - M_BLEND) < 0.02) {
        gl_FragColor = texture2D(tex,  coord);
        return;
    }

    if (abs(mode - M_OPAQUE) < 0.03) {
        gl_FragColor = texture2D(tex,  coord);
        gl_FragColor.a = 1.0;
        return;
    }

    if (abs(mode - M_RALPHA) < 0.04) {
        gl_FragColor = texture2D(tex,  coord);
        gl_FragColor.rgb = color.rgb;
        return;
    }

    gl_FragColor.rg = coord;
    gl_FragColor.ba = vec2(0,1);
}
