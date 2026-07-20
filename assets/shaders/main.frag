#version 450

layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec3 FragPos;
layout (location = 1) in vec3 normal;

void main() {
    const vec3 lightDir = vec3(-1, -1, -1);
    const float ambient = .2;

    vec3 L = normalize(-lightDir);
    float NdotL = max(dot(normalize(normal), L), 0.0);

    FragColor.rgb = vec3(NdotL + ambient);
    FragColor.a = 1.0;
}