#version 450

layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec3 FragPos;

void main() {
    const vec3 lightDir = vec3(-1, -1, -1);
    FragColor.rgb = vec3(1.0) * dot(-lightDir, FragPos);
    // FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}