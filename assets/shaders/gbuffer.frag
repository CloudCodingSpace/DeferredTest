#version 450

#extension GL_EXT_scalar_block_layout : require

layout (location = 0) out vec4 pos;
layout (location = 1) out vec4 normal;

layout (location = 0) in vec3 FragPos;
layout (location = 1) in vec3 oNormal;

const float NEAR_PLANE = 0.01;
const float FAR_PLANE = 1000.0;

float linearizeDepth(float depth) {
	float z = depth * 2.0f - 1.0f; 
	return (2.0f * NEAR_PLANE * FAR_PLANE) / (FAR_PLANE + NEAR_PLANE - z * (FAR_PLANE - NEAR_PLANE));	
}

void main() {
    pos = vec4(FragPos, linearizeDepth(gl_FragCoord.z));
    normal = vec4(oNormal, 0.0);
}