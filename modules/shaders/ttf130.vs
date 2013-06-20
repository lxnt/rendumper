#version 130
#line 2 0

// lt rt rb lb

uniform vec2 vp_size;   // viewport size
uniform vec2 text_size; // text texalbum size

attribute  vec4 position;  // position and unnormalized texcoords
attribute ivec2 stuff_in;  // frag shader stuff

out vec2 coord;
flat out ivec2 stuff;

void main() {
    vec2 posn = 2.0 * vec2(position.xy) / vec2(vp_size) - 1.0;
    gl_Position = vec4(posn.x, -posn.y, 0.0, 1.0);
    coord = vec2(position.zw/text_size);
    stuff = stuff_in;
}
