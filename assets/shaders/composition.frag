#version 450

#extension GL_EXT_scalar_block_layout : require

layout (input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput positionAttachment;
layout (input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput normalAttachment;

layout (location = 0) out vec4 FragColor;

struct Light {
    vec3 pos;
    vec3 color;
    float intensity;
};

layout (set = 0, binding = 0, scalar) buffer LightSSBO {
    uint count;
    Light lights[];
} lightSsbo;

void main() {
    const float ambient = .2;
    vec3 FragPos = subpassLoad(positionAttachment).rgb;
    vec3 normal = subpassLoad(normalAttachment).rgb;

    if (subpassLoad(positionAttachment).a == 1.0) {
        FragColor = vec4(0.1, 0.1, 0.1, 1.0);
        return;
    }

    vec3 totalLight = vec3(0.0);

    for(uint i = 0; i < lightSsbo.count; i++) {
        Light light = lightSsbo.lights[i];
        vec3 L = light.pos - FragPos;
        float attenuation = 1 / dot(L, L);
        L = -normalize(L);

        float NdotL = max(dot(normalize(normal), L), 0.0);
        totalLight += light.color * NdotL * attenuation * light.intensity;
    }

    FragColor.rgb = vec3(totalLight + ambient);
    FragColor.a = 1.0;
}