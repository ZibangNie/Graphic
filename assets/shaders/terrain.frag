#version 330 core
in vec2 vUV;
in float vH;
in vec3 vWorldPos;
in vec3 vNormalW;
out vec4 FragColor;

uniform sampler2D uRocky;
uniform sampler2D uSand;

uniform float uSandHeight;
uniform float uBlendWidth;

// Phase A lighting
uniform vec3  uSunDir;          // “指向太阳”的方向（world）
uniform vec3  uSunColor;
uniform float uSunIntensity;

uniform vec3  uAmbientColor;
uniform float uAmbientIntensity;

uniform vec3  uCameraPos;

void main() {
    vec3 rocky = texture(uRocky, vUV).rgb;
    vec3 sand  = texture(uSand,  vUV).rgb;

    float t = smoothstep(uSandHeight, uSandHeight + uBlendWidth, vH);
    vec3 albedo = mix(sand, rocky, t);

    vec3 N = normalize(vNormalW);
    vec3 L = normalize(uSunDir);                // 入射光方向
    float NdotL = max(dot(N, L), 0.0);

    // 更“白天”的强度
    vec3 ambient = albedo * uAmbientColor * uAmbientIntensity;
    vec3 diffuse = albedo * uSunColor * uSunIntensity * NdotL;

    vec3 color = ambient + diffuse;

    // gamma：立刻提升“白天感”
    color = pow(color, vec3(1.0/2.2));

    FragColor = vec4(color, 1.0);
}