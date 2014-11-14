#version 130
#line 2 0

// see shadertoy.com

// lt rt rb lb

uniform vec3 iResolution;

in vec4 position;

out vec2 fx_FragCoord;

void main() {
    vec2 posn = 2.0 * vec2(position.xy) / vec2(iResolution.xy) - 1.0;
    gl_Position = vec4(posn.x, -posn.y, 0.0, 1.0);

    fx_FragCoord = vec2(position.zw);
}
