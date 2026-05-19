#version 450
#pragma optimize(off)
#extension GL_KHR_vulkan_glsl : enable
layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec3 fragTangent;    
layout(location = 4) in vec3 fragBitangent;   

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 view;
    mat4 proj;
    mat4 invView;
    mat4 invProj; 
    float time;
} global;

struct Light {
    vec4 position;  
    vec4 color;     
};

layout(set = 1, binding = 1) uniform LightingUBO {
    Light lights;
    uint lightCount;
    float ambientStrength;
} lighting;

layout(set = 2, binding = 0) uniform sampler2D armMap;
layout(set = 2, binding = 1) uniform sampler2D albedoMap;
layout(set = 2, binding = 2) uniform sampler2D normalMap;
layout(set = 2, binding = 3) uniform samplerCube uIrradianceMap;
layout(set = 2, binding = 4) uniform samplerCube uPrefilteredMap;
layout(set = 2, binding = 5) uniform sampler2D   uBrdfLut;

const float PREFILTER_MAX_LOD = 5.0;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 FresnelSchlick(float cosTheta, vec3 F0);

vec3 ScreenSpaceDither(vec2 vScreenPos);

void main() {
    vec4 albedoSample  = texture(albedoMap, fragTexCoord);
    vec4 armSample     = texture(armMap,    fragTexCoord);
    vec4 normalSample  = texture(normalMap, fragTexCoord);

    float ao         = armSample.r;        // AO 仍然可以用
    float roughness  = 0.2;                // 固定光滑度，或仍从 g 通道读：armSample.g
    float metallic   = 0.5; 

    vec3 tangentNormal = normalSample.rgb * 2.0 - 1.0;
    vec3 T = normalize(fragTangent);
    vec3 B = normalize(fragBitangent);
    vec3 N_world = normalize(fragNormal);
    mat3 TBN = mat3(T, B, N_world);
    vec3 N = normalize(TBN * tangentNormal);

    vec3 V = normalize(global.invView[3].xyz - fragWorldPos);
    vec3 F0 = mix(vec3(0.04), albedoSample.rgb, metallic);
    vec3 Lo = vec3(0.0);

    // 直接光照（单个光源）
    Light light = lighting.lights;
    vec3 L;
    vec3 radiance = light.color.rgb;
    if (light.position.w == 0.0) {
        L = normalize(-light.position.xyz);
    } else {
        vec3 lightVec = light.position.xyz - fragWorldPos;
        float dist = length(lightVec);
        L = lightVec / dist;
        float attenuation = 1.0 / (dist * dist + 0.001);
        radiance *= attenuation;
    }
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);
    
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    
    vec3 nominator = NDF * G * F;
    float denominator = max(4.0 * NdotV * NdotL, 0.0001);
    vec3 specular = nominator / denominator;
    
    Lo += (kD * albedoSample.rgb / PI + specular) * radiance * NdotL;
    
    // ========== IBL 环境光 ==========
    // 漫反射 Irradiance
    vec3 irradiance = texture(uIrradianceMap, N).rgb;
    //vec3 irradiance = textureLod(uPrefilteredMap, N, PREFILTER_MAX_LOD - 1.0).rgb;
    vec3 diffuseIBL = irradiance * albedoSample.rgb;
    
    // 镜面反射 Prefiltered + BRDF LUT
    vec3 R = reflect(-V, N);
    float roughnessLevel = roughness * 5.0;   // maxLod = 5
    vec3 prefilteredColor = textureLod(uPrefilteredMap, R, roughnessLevel).rgb;
    vec2 brdfParams = texture(uBrdfLut, vec2(NdotV, roughness)).rg;
    vec3 specularIBL = prefilteredColor * (kS * brdfParams.x + brdfParams.y);
    
    vec3 ambientIBL = (diffuseIBL * kD + specularIBL) * ao;
    
    vec3 color = Lo + ambientIBL;
    
     //if (diffuseIBL.x < 0.0) discard;  

    // 抖动 + 色调映射 + Gamma
    vec2 screenPos = gl_FragCoord.xy;
    vec3 dither = ScreenSpaceDither(screenPos);
    color += dither;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));
    
    outColor = vec4(color, 1.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float alpha = roughness * roughness;  
    float a2 = alpha * alpha;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (a2 - 1.0) + 1.0;             
    denom = PI * denom * denom;
    return a2 / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float alpha = roughness * roughness;
    float k = (alpha + 1.0) * (alpha + 1.0) / 8.0; 
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 ScreenSpaceDither(vec2 vScreenPos) {
    // 使用屏幕坐标和时间的点积生成伪随机数
    vec3 vDither = dot(vec2(171.0, 231.0), vScreenPos.xy + global.time).xxx;
    vDither.rgb = fract(vDither.rgb / vec3(103.0, 71.0, 97.0)) - vec3(0.5);
    return (vDither.rgb / 255.0) * 0.375; 
}