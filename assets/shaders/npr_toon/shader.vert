#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;   
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorldPos; 

// set 0：全局 UBO（view / proj）
layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 view;
    mat4 proj;
} global;

// push constant：模型矩阵
layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;

    fragTexCoord = inTexCoord;
    fragNormal = mat3(pc.model) * inNormal;  

    gl_Position = global.proj * global.view * pc.model * vec4(inPosition, 1.0);
}