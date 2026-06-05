#version 450

const vec2 pos[3] = vec2[3] (
    vec2(-0.5, -0.5),
    vec2( 0.5, -0.5),
    vec2( 0.0,  0.5)
);

void main() {
    vec2 position = pos[gl_VertexIndex];
    position.y *= -1.0f;
    gl_Position = vec4(position, 0.0, 1.0);
}