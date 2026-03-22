#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 view;
    mat4 proj;
};

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    gl_Position = proj * view * worldPos;
    fragTexCoord = inTexCoord;
    fragNormal = mat3(pc.model) * inNormal; 
}