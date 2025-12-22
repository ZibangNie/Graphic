#version 330 core
layout(location=0) in vec3 aPos;     // y 可为 0
layout(location=1) in vec3 aNormal;  // (0,1,0)
layout(location=2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

uniform float uTime;
uniform float uWaterY;

out vec3 vWorldPos;
out vec3 vNormalW;

float waveHeight(vec2 p, float t, out vec2 dHdXZ)
{
    float h = 0.0;
    dHdXZ = vec2(0.0);

    // 注意：世界坐标很大时，频率别太高
    vec2 d1 = normalize(vec2(1.0, 0.3));
    float k1 = 0.10;
    float s1 = 0.80;
    float a1 = 0.10;
    float ph1 = dot(d1, p) * k1 + t * s1;
    h += a1 * sin(ph1);
    dHdXZ += (a1 * cos(ph1) * k1) * d1;

    vec2 d2 = normalize(vec2(-0.4, 1.0));
    float k2 = 0.16;
    float s2 = 1.10;
    float a2 = 0.06;
    float ph2 = dot(d2, p) * k2 + t * s2;
    h += a2 * sin(ph2);
    dHdXZ += (a2 * cos(ph2) * k2) * d2;

    vec2 d3 = normalize(vec2(0.7, -0.9));
    float k3 = 0.22;
    float s3 = 1.60;
    float a3 = 0.03;
    float ph3 = dot(d3, p) * k3 + t * s3;
    h += a3 * sin(ph3);
    dHdXZ += (a3 * cos(ph3) * k3) * d3;

    return h;
}

void main()
{
    // 先构造平面世界坐标（y 由 uWaterY 控制）
    vec4 wp = uModel * vec4(aPos.x, 0.0, aPos.z, 1.0);
    wp.y = uWaterY;

    // 波浪位移：使用世界 xz
    vec2 dHdXZ;
    float h = waveHeight(wp.xz, uTime, dHdXZ);
    wp.y += h;

    // 法线：(-dH/dx, 1, -dH/dz)
    vec3 N = normalize(vec3(-dHdXZ.x, 1.0, -dHdXZ.y));

    vWorldPos = wp.xyz;
    vNormalW = normalize(mat3(transpose(inverse(uModel))) * N);

    gl_Position = uProj * uView * wp;
}
