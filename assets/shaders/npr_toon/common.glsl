// ── NPR Toon 公共函数库（各 shader include 用）──
// include guard：GLSL 的 #version 必须在文件首行，故 guard 宏不能包住 #version，
// 只能在被包含文件内用 #ifndef（被包含文件本身不含 #version）。
#ifndef NPR_TOON_COMMON_GLSL
#define NPR_TOON_COMMON_GLSL

vec2 toFloat2(float value){
    return vec2(value,value);
}

vec3 toFloat3(float value){
    return vec3(value,value,value);
}

vec4 toFloat4(float value){
    return vec4(toFloat3(value),1.0);
}

vec3 HalfwayVecter(vec3 v,vec3 l){
    return normalize(v+l);
}

float halfLambert(vec3 normal, vec3 L) {
    float NOL = dot(normal, L);
    return NOL * 0.5 + 0.5;
}

float blinnPhong(vec3 N,vec3 H,float shinniness){
    return pow(max(dot(N,H),0),shinniness);
}

#endif // NPR_TOON_COMMON_GLSL
