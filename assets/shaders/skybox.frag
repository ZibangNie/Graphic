#version 330 core
out vec4 FragColor;

in vec3 vDir;

uniform samplerCube uSkyDay;
uniform samplerCube uSkyNight;

uniform vec3  uSunDir;     // 指向“太阳在天空的方向”（注意和光照 direction 可能相反）
uniform float uDayFactor;  // 0=夜晚, 1=白天
uniform float uStarRot;    // 星图旋转角（弧度）

mat3 rotY(float a) {
    float c = cos(a), s = sin(a);
    return mat3(
    c, 0, -s,
    0, 1,  0,
    s, 0,  c
    );
}

vec3 tonemapReinhard(vec3 x) {
    return x / (x + vec3(1.0));
}

void main() {
    vec3 dir = normalize(vDir);

    // 夜晚星空做一点旋转
    vec3 dirNight = rotY(uStarRot) * dir;

    vec3 colDay   = texture(uSkyDay, dir).rgb;
    vec3 colNight = texture(uSkyNight, dirNight).rgb;
    colNight *= 0.10;   // 0.10 ~ 0.40 自己调

    // 日夜混合
    vec3 col = mix(colNight, colDay, uDayFactor);

    // 太阳圆盘（让太阳“真实存在”）
    // 太阳半径（角度越小太阳越小）
    float sunCosOuter = cos(radians(0.65)); // 外圈
    float sunCosInner = cos(radians(0.25)); // 内圈更亮
    float sd = dot(dir, normalize(uSunDir));

    float discOuter = smoothstep(sunCosOuter, 1.0, sd);
    float discInner = smoothstep(sunCosInner, 1.0, sd);

    // 太阳颜色：白天偏白，地平线更暖（这里用 (1-uDayFactor) 与 discOuter 简单调一下）
    vec3 sunColor = mix(vec3(1.0, 0.55, 0.25), vec3(1.0, 0.98, 0.92), uDayFactor);
    col += sunColor * (discOuter * 2.5 + discInner * 6.0) * uDayFactor;

    // 轻微暗角/地平线压暗（让日出日落更有层次）
    float horizon = 1.0 - clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
    col *= mix(1.0, 0.90, horizon * 0.35);

    // HDR -> LDR（你现在管线大概率是 LDR）
    col = tonemapReinhard(col);
    col = pow(col, vec3(1.0/2.2));

    FragColor = vec4(col, 1.0);


}
