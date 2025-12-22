#version 330 core
in vec3 vWorldPos;
in vec3 vNormalW;
out vec4 FragColor;

// 与 LightingSystem::applyFromEnvironment 对齐
uniform vec3  uSunDir;
uniform vec3  uSunColor;
uniform float uSunIntensity;

uniform vec3  uAmbientColor;
uniform float uAmbientIntensity;

uniform vec3  uCameraPos;

// 反射
uniform sampler2D uReflectTex;
uniform mat4 uViewRef;   // 反射相机 view
uniform mat4 uProj;      // 与主相机同一个 proj

// 水参数
uniform vec3  uWaterColor;        // 建议 vec3(0.02,0.15,0.22)
uniform float uReflectStrength;   // 建议 1.0
uniform float uDistortStrength;   // 建议 0.02

vec2 projectToUV(mat4 V, mat4 P, vec3 worldPos)
{
    vec4 clip = P * V * vec4(worldPos, 1.0);
    vec2 ndc = clip.xy / clip.w;      // [-1,1]
    return ndc * 0.5 + 0.5;           // [0,1]
}

void main()
{
    vec3 N = normalize(vNormalW);
    vec3 V = normalize(uCameraPos - vWorldPos);

    // 你的工程里 uSunDir 是“指向太阳”的方向，terrain/basic 都是用它当 L
    vec3 L = normalize(uSunDir);

    float NdotL = max(dot(N, L), 0.0);

    // Fresnel（Schlick）
    float cosNV = clamp(dot(N, V), 0.0, 1.0);
    float fres = pow(1.0 - cosNV, 5.0);

    // 高光（Blinn-Phong）
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 128.0);

    vec3 ambient = uWaterColor * uAmbientColor * uAmbientIntensity;
    vec3 diffuse = uWaterColor * uSunColor * uSunIntensity * (0.35 * NdotL);
    vec3 highlight = uSunColor * uSunIntensity * (0.25 * spec);

    // 反射采样坐标：用反射相机把“当前水面点”投影到反射贴图上
    vec2 uv = projectToUV(uViewRef, uProj, vWorldPos);
    uv.y = 1.0 - uv.y; // 常见需要翻转（与你的 FBO 输出约定对齐）

    // 用法线 xz 轻微扭曲反射
    uv += N.xz * uDistortStrength;
    uv = clamp(uv, vec2(0.001), vec2(0.999));

    vec3 refl = texture(uReflectTex, uv).rgb;

    vec3 base = ambient + diffuse + highlight;

    // fres 越大（掠射角）越反光
    vec3 color = mix(base, refl, clamp(fres * uReflectStrength, 0.0, 1.0));

    // 与 terrain.frag 保持一致的 gamma
    color = pow(color, vec3(1.0/2.2));
    FragColor = vec4(color, 1.0);
}
