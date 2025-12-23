/*
 * skybox.frag
 *
 * Purpose:
 *   Fragment shader for rendering a skybox with a day/night cubemap blend, optional starfield rotation,
 *   and a procedural sun disk overlay. Performs a simple HDR-to-LDR tonemap and gamma correction.
 *
 * Inputs:
 *   - vDir : Direction vector from the camera through the skybox fragment (typically in view or world space;
 *           must match the convention used when sampling the cubemaps). Normalized in-shader.
 *
 * Uniforms:
 *   - uSkyDay   : Daytime cubemap sampler.
 *   - uSkyNight : Nighttime cubemap sampler (stars). Rotated around Y by uStarRot.
 *
 *   - uSunDir    : Sun direction in the same space as vDir. Represents the direction *toward the sun in the sky*.
 *                 (In many lighting setups, the directional light vector may use the opposite convention.)
 *   - uDayFactor : Day/night blend factor: 0.0 = full night, 1.0 = full day. Expected range [0,1].
 *   - uStarRot   : Starfield rotation angle in radians around the Y axis. Typical range [0, 2*pi].
 *
 * Outputs:
 *   - FragColor : Final LDR sky color in gamma space (approx. sRGB), alpha = 1.0.
 *
 * Notes:
 *   - The night cubemap intensity is scaled down to control star brightness.
 *   - The sun disk is implemented by thresholding dot(dir, uSunDir) against angular radii.
 *   - Tonemapping uses a basic Reinhard operator, followed by gamma correction (1/2.2).
 */

#version 330 core
out vec4 FragColor;

in vec3 vDir;

uniform samplerCube uSkyDay;
uniform samplerCube uSkyNight;

uniform vec3  uSunDir;     // Sun direction in sky space; may be opposite of directional-light vector.
uniform float uDayFactor;  // Day/night blend: 0=night, 1=day.
uniform float uStarRot;    // Starfield rotation angle (radians).

/*
 * Rotation matrix around the Y axis.
 *
 * Parameters:
 *   a : Angle in radians.
 *
 * Returns:
 *   3x3 rotation matrix for rotating direction vectors.
 */
mat3 rotY(float a) {
    float c = cos(a), s = sin(a);
    return mat3(
    c, 0, -s,
    0, 1,  0,
    s, 0,  c
    );
}

/*
 * Simple Reinhard tonemap operator for HDR -> LDR compression.
 *
 * Parameters:
 *   x : HDR linear RGB.
 *
 * Returns:
 *   Tonemapped linear RGB in approximately [0,1).
 */
vec3 tonemapReinhard(vec3 x) {
    return x / (x + vec3(1.0));
}

void main() {
    // Normalize incoming direction to ensure stable cubemap sampling and sun-disk math.
    vec3 dir = normalize(vDir);

    // Rotate only the night sky to allow independent starfield rotation.
    vec3 dirNight = rotY(uStarRot) * dir;

    // Sample cubemaps.
    vec3 colDay   = texture(uSkyDay, dir).rgb;
    vec3 colNight = texture(uSkyNight, dirNight).rgb;

    // Star brightness control (scene-dependent). Typical tuning range ~[0.10, 0.40].
    colNight *= 0.10;

    // Blend night -> day using uDayFactor.
    vec3 col = mix(colNight, colDay, uDayFactor);

    // --- Procedural sun disk overlay ---
    // sunCosOuter/Inner define angular radii (smaller angle => smaller disk).
    float sunCosOuter = cos(radians(0.65)); // Outer disk radius.
    float sunCosInner = cos(radians(0.25)); // Inner brighter core.

    // sd is cosine of angle between view direction and sun direction.
    float sd = dot(dir, normalize(uSunDir));

    // Smooth thresholds create soft edges around the disk.
    float discOuter = smoothstep(sunCosOuter, 1.0, sd);
    float discInner = smoothstep(sunCosInner, 1.0, sd);

    // Sun tint: warmer near horizon/transition, whiter at full day.
    // uDayFactor also gates contribution so sun is suppressed at night.
    vec3 sunColor = mix(vec3(1.0, 0.55, 0.25), vec3(1.0, 0.98, 0.92), uDayFactor);
    col += sunColor * (discOuter * 2.5 + discInner * 6.0) * uDayFactor;

    // Subtle horizon darkening to increase depth during sunrise/sunset.
    float horizon = 1.0 - clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
    col *= mix(1.0, 0.90, horizon * 0.35);

    // HDR -> LDR and gamma correction (approximate sRGB output).
    col = tonemapReinhard(col);
    col = pow(col, vec3(1.0/2.2));

    FragColor = vec4(col, 1.0);
}
