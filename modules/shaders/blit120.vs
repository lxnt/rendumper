#version 120
#line 2 0

// lt rt rb lb

uniform ivec2 dstsize;
uniform ivec4 srcrect;
uniform ivec2 srcsize;

attribute vec4 position;

varying vec2 coord;

void main() {
    vec2 posn = 2.0 * vec2(position.xy) / vec2(dstsize) - 1.0;
    gl_Position = vec4(posn.x, posn.y, 0.0, 1.0);

    if (any(lessThanEqual(srcsize, ivec2(0,0))))
        coord = vec2(position.zw);
    else
        coord = (position.zw * srcrect.zw + srcrect.xy) / srcsize;
}
