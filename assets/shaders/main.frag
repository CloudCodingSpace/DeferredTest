#version 450

#extension GL_EXT_scalar_block_layout : require

layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec3 FragPos;
layout (location = 1) in vec3 normal;

struct Light {
    vec3 pos;
    vec3 color;
};

layout (set = 0, binding = 0, scalar) buffer LightSSBO {
    uint count;
    Light lights[];
} lightSsbo;

void main() {
    const float ambient = .2;

    vec3 totalLight = vec3(0.0);

    for(uint i = 0; i < lightSsbo.count; i++) {
        Light light = lightSsbo.lights[i];
        vec3 L = normalize(FragPos - light.pos);
        float attenuation = 1 / dot(L, L);

        float NdotL = max(dot(normalize(normal), L), 0.0);
        totalLight += light.color * NdotL * attenuation;
    }

    FragColor.rgb = vec3(totalLight + ambient);
    FragColor.a = 1.0;
}