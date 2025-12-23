#version 330 core

in vec2 vUV;
in float vAge;   // 0 -> born, 1 -> dead
in float vSeed;  // 0..1
in float vKind;  // 0 flame, 1 ember, 2 glow

out vec4 FragColor;

uniform float uTime;
uniform float uIntensity;

// --- small procedural noise ---
float hash12(vec2 p) {
    vec3 p3  = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float noise2(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash12(i);
    float b = hash12(i + vec2(1.0, 0.0));
    float c = hash12(i + vec2(0.0, 1.0));
    float d = hash12(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; ++i) {
        v += a * noise2(p);
        p *= 2.03;
        a *= 0.5;
    }
    return v;
}

float saturate(float x) { return clamp(x, 0.0, 1.0); }

void main() {
    vec2 uv = vUV;
    vec2 c = uv - vec2(0.5, 0.5);

    float r = length(c);
    float radial = smoothstep(0.55, 0.0, r);
    float base = smoothstep(1.0, 0.0, uv.y);

    float n = fbm(vec2(uv.x * 3.2, uv.y * 4.6 - uTime * 2.2) + vec2(vSeed * 10.0, 0.0));

    // Only flames: stronger distortion
    if (vKind < 0.5) {
        float wobble = (n - 0.5);
        uv.x += wobble * 0.16 * (1.0 - uv.y);
        uv.y += wobble * 0.08;
    }

    float yCut = uv.y + (n - 0.5) * 0.22;
    float vertical = smoothstep(1.08, 0.0, yCut);
    float shape = radial * vertical;

    float flick = 0.86 + 0.14 * sin(uTime * 9.0 + vSeed * 40.0);

    vec3 color;
    float alpha;

    if (vKind < 0.5) {
        // Flame
        float age = saturate(vAge);
        float hot = saturate((0.95 * base + 0.30) * (1.0 - age));

        vec3 cHot  = vec3(1.70, 1.30, 0.55);
        vec3 cWarm = vec3(1.35, 0.45, 0.08);
        vec3 cDim  = vec3(0.18, 0.06, 0.03);

        color = mix(cWarm, cHot, hot);
        color = mix(color, cDim, smoothstep(0.55, 1.0, age));

        float core = smoothstep(0.22, 0.0, r) * (0.7 + 0.3 * base);
        color *= (1.0 + 0.8 * core);

        alpha = shape * (0.85 + 0.45 * base) * (1.0 - smoothstep(0.65, 1.0, age));
        alpha *= flick;

    } else if (vKind < 1.5) {
        // Embers
        float age = saturate(vAge);

        float spark = smoothstep(0.18, 0.0, r);
        spark *= smoothstep(0.25, 0.0, abs(uv.y - 0.5));

        vec3 cSpark = vec3(1.85, 1.10, 0.25);
        vec3 cAsh   = vec3(0.45, 0.15, 0.05);

        color = mix(cSpark, cAsh, smoothstep(0.0, 1.0, age));
        alpha = spark * (1.0 - age) * (0.55 + 0.45 * flick);

    } else {
        // Ground glow (subtle)
        float age = saturate(vAge);

        float disk = smoothstep(0.55, 0.0, r);
        disk *= smoothstep(0.10, 0.0, abs(uv.y - 0.5));

        vec3 cGlow = vec3(1.25, 0.45, 0.10);
        color = cGlow * (0.55 + 0.25 * flick);
        alpha = disk * (1.0 - age) * 0.55;
    }

    alpha *= uIntensity;
    color *= uIntensity;

    if (alpha < 0.004) discard;

    FragColor = vec4(color, alpha);
}
