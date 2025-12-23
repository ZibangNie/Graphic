/*
 * particle.frag
 *
 * Purpose:
 *   Fragment shader for a small campfire particle system with three visual kinds:
 *     - Flame (vKind ~ 0)
 *     - Ember (vKind ~ 1)
 *     - Ground glow (vKind ~ 2)
 *
 *   Generates procedural shape, distortion, and flicker entirely in shader code (no textures).
 *   Outputs premultiplied-style intensity scaling (color and alpha scaled by uIntensity).
 *
 * Inputs:
 *   - vUV   : Particle-local UV coordinates, typically [0,1] across the billboard quad.
 *   - vAge  : Normalized lifetime (0 = birth, 1 = death). Expected range [0,1].
 *   - vSeed : Per-particle random seed in [0,1] to decorrelate noise/flicker.
 *   - vKind : Particle type selector:
 *             < 0.5  -> flame
 *             < 1.5  -> ember
 *             else   -> glow
 *
 * Uniforms:
 *   - uTime      : Time in seconds (continuous).
 *   - uIntensity : Global scalar for overall brightness/opacity. Typical range [0,1], can exceed for stylized output.
 *
 * Outputs:
 *   - FragColor : RGBA with alpha used for blending. Uses discard for very low alpha to reduce overdraw.
 *
 * Notes:
 *   - Procedural noise is lightweight and deterministic per fragment; fbm() uses 4 octaves.
 *   - Flame kind applies stronger UV distortion to create animated tongues of fire.
 *   - Color values intentionally exceed 1.0 for a pseudo-HDR look; final appearance depends on tone mapping / blending.
 */

#version 330 core

in vec2 vUV;
in float vAge;   // Normalized lifetime: 0 -> born, 1 -> dead.
in float vSeed;  // Per-particle seed: expected 0..1.
in float vKind;  // Type selector: 0 flame, 1 ember, 2 glow.

out vec4 FragColor;

uniform float uTime;       // Time in seconds.
uniform float uIntensity;  // Global intensity multiplier.

// --- small procedural noise ---
// hash12: 2D -> 1D hash in [0,1), suitable for value-noise construction.
float hash12(vec2 p) {
    vec3 p3  = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

// noise2: Smoothed 2D value noise in [0,1] using hash12 at lattice points.
float noise2(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash12(i);
    float b = hash12(i + vec2(1.0, 0.0));
    float c = hash12(i + vec2(0.0, 1.0));
    float d = hash12(i + vec2(1.0, 1.0));

    // Hermite smoothing for interpolation weights.
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

// fbm: Fractal Brownian motion using 4 octaves of noise2.
// Returns roughly [0,1] with increased mid-frequency detail.
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

// saturate: Clamp to [0,1] (HLSL-style helper).
float saturate(float x) { return clamp(x, 0.0, 1.0); }

void main() {
    // Particle-local coordinates and centered coordinates.
    vec2 uv = vUV;
    vec2 c = uv - vec2(0.5, 0.5);

    // Radial falloff for a soft-edged sprite.
    float r = length(c);
    float radial = smoothstep(0.55, 0.0, r);

    // Vertical ramp; base is strongest near the bottom (uv.y ~ 0).
    float base = smoothstep(1.0, 0.0, uv.y);

    // Animated FBM field; time scrolls upward (negative Y) to simulate rising motion.
    // vSeed offsets X to decorrelate particles.
    float n = fbm(vec2(uv.x * 3.2, uv.y * 4.6 - uTime * 2.2) + vec2(vSeed * 10.0, 0.0));

    // Only flames: stronger distortion to create flickering tongues.
    if (vKind < 0.5) {
        float wobble = (n - 0.5);             // Center noise around 0.
        uv.x += wobble * 0.16 * (1.0 - uv.y); // Stronger near the base, weaker at the top.
        uv.y += wobble * 0.08;
    }

    // Vertical cutoff modulated by noise; values above 1 reduce the top edge.
    float yCut = uv.y + (n - 0.5) * 0.22;
    float vertical = smoothstep(1.08, 0.0, yCut);

    // Combined sprite shape mask.
    float shape = radial * vertical;

    // Global flicker term; per-particle phase is driven by vSeed.
    float flick = 0.86 + 0.14 * sin(uTime * 9.0 + vSeed * 40.0);

    vec3 color;
    float alpha;

    if (vKind < 0.5) {
        // Flame
        float age = saturate(vAge);

        // "Hotness" is highest near the base and early in life.
        float hot = saturate((0.95 * base + 0.30) * (1.0 - age));

        // Warm palette; values > 1.0 increase perceived brightness under tone mapping.
        vec3 cHot  = vec3(1.70, 1.30, 0.55);
        vec3 cWarm = vec3(1.35, 0.45, 0.08);
        vec3 cDim  = vec3(0.18, 0.06, 0.03);

        // Blend warm -> hot, then fade toward dim as particle ages.
        color = mix(cWarm, cHot, hot);
        color = mix(color, cDim, smoothstep(0.55, 1.0, age));

        // Inner core boost to emphasize a brighter center.
        float core = smoothstep(0.22, 0.0, r) * (0.7 + 0.3 * base);
        color *= (1.0 + 0.8 * core);

        // Alpha fades out with age and is stronger near the bottom.
        alpha = shape * (0.85 + 0.45 * base) * (1.0 - smoothstep(0.65, 1.0, age));
        alpha *= flick;

    } else if (vKind < 1.5) {
        // Embers
        float age = saturate(vAge);

        // Small spark-like mask concentrated near the center.
        float spark = smoothstep(0.18, 0.0, r);
        spark *= smoothstep(0.25, 0.0, abs(uv.y - 0.5));

        vec3 cSpark = vec3(1.85, 1.10, 0.25);
        vec3 cAsh   = vec3(0.45, 0.15, 0.05);

        // Embers cool as they age; alpha decays to zero.
        color = mix(cSpark, cAsh, smoothstep(0.0, 1.0, age));
        alpha = spark * (1.0 - age) * (0.55 + 0.45 * flick);

    } else {
        // Ground glow (subtle)
        float age = saturate(vAge);

        // Thin horizontal disk around the sprite center.
        float disk = smoothstep(0.55, 0.0, r);
        disk *= smoothstep(0.10, 0.0, abs(uv.y - 0.5));

        vec3 cGlow = vec3(1.25, 0.45, 0.10);
        color = cGlow * (0.55 + 0.25 * flick);
        alpha = disk * (1.0 - age) * 0.55;
    }

    // Global intensity scaling (affects both perceived brightness and opacity).
    alpha *= uIntensity;
    color *= uIntensity;

    // Early-out to reduce overdraw on near-transparent fragments.
    if (alpha < 0.004) discard;

    FragColor = vec4(color, alpha);
}
