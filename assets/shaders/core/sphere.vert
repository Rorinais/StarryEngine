#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;   
layout(location = 2) in vec2 inTexCoord;
//layout(location = 3) in vec4 inInstanceMatrix0;
//layout(location = 4) in vec4 inInstanceMatrix1;
//layout(location = 5) in vec4 inInstanceMatrix2;
//layout(location = 6) in vec4 inInstanceMatrix3;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragNormal;

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
    //mat4 instanceMatrix = mat4(
    //    inInstanceMatrix0,
    //   inInstanceMatrix1,
    //    inInstanceMatrix2,
    //    inInstanceMatrix3
    //);
    //vec4 worldPos = instanceMatrix * vec4(inPosition, 1.0);
    //gl_Position = global.proj * global.view * worldPos;
    gl_Position = global.proj * global.view * pc.model* vec4(inPosition, 1.0);

    fragTexCoord = inTexCoord;
    fragNormal = mat3(pc.model) * inNormal; 
}