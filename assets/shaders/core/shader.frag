#version 450
#extension GL_KHR_vulkan_glsl : enable
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D texSampler;

float halfLambert(vec3 Nomal,vec3 L){
    float NOL=dot(Nomal,L);
    float halfLambert=NOL*0.5+0.5;
    return halfLambert;
}

void main() {
    vec3 L=vec3(1.0,0.5,0.0);
    float halfLambert= halfLambert(fragNormal,L);

    vec3 shadowColor=vec3(texture(texSampler, fragTexCoord).xyz)*0.5;


    outColor = texture(texSampler, fragTexCoord);
}