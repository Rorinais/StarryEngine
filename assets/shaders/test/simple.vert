#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inTangent;   

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out vec3 fragTangent;      
layout(location = 4) out vec3 fragBitangent;   

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 view;
    mat4 proj;
    mat4 invView;
    mat4 invProj;   
} global;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);

    fragTexCoord = inTexCoord;
    
    mat3 normalMatrix = mat3(transpose(inverse(pc.model)));
    fragNormal = normalize(normalMatrix * inNormal);
    
    fragTangent = normalize(mat3(pc.model) * inTangent.xyz);
    
    fragBitangent = cross(fragNormal, fragTangent) * inTangent.w;
    
    fragWorldPos = worldPos.xyz;
    gl_Position = global.proj * global.view * worldPos;
}