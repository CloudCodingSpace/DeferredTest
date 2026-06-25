#version 450

layout (location = 0) in vec3 pos;

layout (push_constant) uniform PushConstant {
    mat4 vp;
    mat4 model;
} pc;

layout (location = 0) out vec3 FragPos;

void main() {
    vec4 worldPos = pc.model * vec4(pos, 1.0);
    gl_Position = pc.vp * worldPos;
    FragPos = worldPos.xyz;
}