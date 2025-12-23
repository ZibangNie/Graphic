/*
 * terrain.frag
 *
 * Purpose:
 *   Fragment shader for terrain rendering with height-based material blending (sand <-> rocky)
 *   and a simple Lambertian (diffuse) + ambient lighting model.
 *
 * Inputs:
 *   - vUV       : Terrain UVs used to sample uRocky and uSand.
 *   - vH        : Height scalar used to drive sand/rock blending (same units as terrain heightfield).
 *   - vWorldPos : World-space position (currently unused; reserved for extensions such as fog).
 *   - vNormalW  : World-space normal (should be normalized or will be normalized here).
 *
 * Uniforms (materials):
 *   - uRocky      : Rocky terrain albedo texture (RGB).
 *   - uSand       : Sand terrain albedo texture (RGB).
 *   - uSandHeight : Height threshold where blending transitions from sand to rocky.
 *   - uBlendWidth : Transition width (in height units). Larger values produce a softer blend.
 *
 * Uniforms (lighting):
 *   - uSunDir        : Sun direction in world space. Interpreted here as the direction *toward the sun*.
 *                      Ensure consistency with the normal dot product convention used by the lighting system.
 *   - uSunColor      : Sun light color (linear RGB).
 *   - uSunIntensity  : Scalar multiplier for sun brightness; typical range ~[0, 10] depending on exposure.
 *   - uAmbientColor  : Ambient light color (linear RGB).
 *   - uAmbientIntensity : Scalar multiplier for ambient; typical range ~[0, 1].
 *   - uCameraPos     : Camera position in world space (currently unused; reserved for specular/fog).
 *
 * Outputs:
 *   - FragColor : Lit terrain color (gamma corrected), alpha = 1.0.
 *
 * Notes:
 *   - Material blend uses smoothstep(uSandHeight, uSandHeight + uBlendWidth, vH).
 *     If uBlendWidth is 0, the blend becomes a hard step.
 *   - Gamma correction is applied in-shader (pow(color, 1/2.2)), implying upstream computations are in linear space.
 *     Ensure textures and the rest of the pipeline are consistent with this assumption to avoid double-gamma issues.
 */

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
uniform vec3  uSunDir;          // Sun direction (world space); convention must match N·L usage.
uniform vec3  uSunColor;
uniform float uSunIntensity;

uniform vec3  uAmbientColor;
uniform float uAmbientIntensity;

uniform vec3  uCameraPos;

void main() {
    // Sample base materials (assumed to be in linear RGB unless textures are authored in sRGB).
    vec3 rocky = texture(uRocky, vUV).rgb;
    vec3 sand  = texture(uSand,  vUV).rgb;

    // Height-based blend factor:
    //   t=0   -> sand
    //   t=1   -> rocky
    float t = smoothstep(uSandHeight, uSandHeight + uBlendWidth, vH);
    vec3 albedo = mix(sand, rocky, t);

    // Lambertian lighting.
    vec3 N = normalize(vNormalW);
    vec3 L = normalize(uSunDir);                // Incident light direction (world space).
    float NdotL = max(dot(N, L), 0.0);

    vec3 ambient = albedo * uAmbientColor * uAmbientIntensity;
    vec3 diffuse = albedo * uSunColor * uSunIntensity * NdotL;

    vec3 color = ambient + diffuse;

    // Gamma correction to approximate sRGB output.
    color = pow(color, vec3(1.0/2.2));

    FragColor = vec4(color, 1.0);
}
