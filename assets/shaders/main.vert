#version 450

// const vec2 pos[3] = vec2[3] (
//     vec2(-0.5, -0.5),
//     vec2( 0.5, -0.5),
//     vec2( 0.0,  0.5)
// );

layout (location = 0) in vec3 pos;

void main() {
    gl_Position = vec4(pos, 1.0);
    gl_Position.y *= -1.0;
}