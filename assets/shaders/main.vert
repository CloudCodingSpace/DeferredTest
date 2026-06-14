#version 450

layout (location = 0) in vec3 pos;

layout (push_constant) uniform PushConstant {
    mat4 vp;
    mat4 model;
} pc;

void main() {
    gl_Position = pc.vp * pc.model * vec4(pos, 1.0);
}