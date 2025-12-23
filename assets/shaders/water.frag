/*
 * water.frag
 *
 * Purpose:
 *   Water surface fragment shader with:
 *     - Ambient + diffuse lighting (Lambertian, scaled down for water)
 *     - Specular highlight (Blinn-Phong)
 *     - Screen-space planar reflection sampled from a reflection render target
 *     - Fresnel blending (Schlick approximation) between base shading and reflection
 *     - Optional normal-based UV distortion for the reflection
 *
 * Inputs:
 *   - vWorldPos : Fragment position in world space (used for reflection projection and view vector).
 *   - vNormalW  : World-space normal (used for lighting, Fresnel, and distortion).
 *
 * Uniforms (lighting; aligned with LightingSystem::applyFromEnvironment):
 *   - uSunDir           : Sun direction in world space. Interpreted as direction *toward the sun* for N·L.
 *   - uSunColor         : Sun light color (linear RGB).
 *   - uSunIntensity     : Sun brightness scalar; typical range ~[0, 10] depending on exposure.
 *   - uAmbientColor     : Ambient light color (linear RGB).
 *   - uAmbientIntensity : Ambient scalar; typical range ~[0, 1].
 *   - uCameraPos        : Camera position in world space.
 *
 * Uniforms (reflection):
 *   - uReflectTex : Reflection color buffer (2D texture) rendered from a mirrored/reflection camera.
 *   - uViewRef    : View matrix of the reflection camera.
 *   - uProj       : Projection matrix used for both main and reflection cameras (must match reflection render).
 *
 * Uniforms (water appearance):
 *   - uWaterColor       : Base water tint (linear RGB). Suggested: vec3(0.02, 0.15, 0.22).
 *   - uReflectStrength  : Reflection strength multiplier. Suggested range [0, 1], can exceed for stylized output.
 *   - uDistortStrength  : Reflection UV distortion strength (world-normal xz). Suggested range ~[0.0, 0.05].
 *
 * Outputs:
 *   - FragColor : Final water color (gamma corrected), alpha = 1.0.
 *
 * Notes:
 *   - projectToUV() projects a world position into reflection texture UV space by applying P*V and mapping NDC [-1,1] to [0,1].
 *   - uv.y flip is a convention that depends on FBO / texture coordinate orientation.
 *   - Fresnel term increases reflectivity at grazing angles (larger fres).
 *   - Gamma correction is applied at the end to match terrain.frag; ensure the rest of the pipeline is consistent to avoid double-gamma.
 */

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
uniform mat4 uViewRef;   // Reflection camera view matrix.
uniform mat4 uProj;      // Projection matrix (must match the reflection render pass).

// 水参数
uniform vec3  uWaterColor;        // Suggested: vec3(0.02,0.15,0.22).
uniform float uReflectStrength;   // Suggested: 1.0 (effective range typically [0,1]).
uniform float uDistortStrength;   // Suggested: 0.02

/*
 * Projects a world-space position into 2D UV coordinates for sampling a screen-aligned render target.
 *
 * Parameters:
 *   V        : View matrix (reflection camera).
 *   P        : Projection matrix.
 *   worldPos : World-space point to project.
 *
 * Returns:
 *   UV in [0,1]^2 derived from NDC coordinates.
 */
vec2 projectToUV(mat4 V, mat4 P, vec3 worldPos)
{
    vec4 clip = P * V * vec4(worldPos, 1.0);
    vec2 ndc = clip.xy / clip.w;      // Normalized device coords in [-1,1].
    return ndc * 0.5 + 0.5;           // Map to [0,1] for texture sampling.
}

void main()
{
    // Normalize basis vectors.
    vec3 N = normalize(vNormalW);
    vec3 V = normalize(uCameraPos - vWorldPos); // View direction from surface to camera.

    // Sun direction convention: treated as incident light direction for N·L (as in terrain/basic).
    vec3 L = normalize(uSunDir);

    float NdotL = max(dot(N, L), 0.0);

    // Fresnel reflectance (Schlick approximation).
    // Higher at grazing angles (cosNV -> 0), lower at normal incidence.
    float cosNV = clamp(dot(N, V), 0.0, 1.0);
    float fres = pow(1.0 - cosNV, 5.0);

    // Specular highlight (Blinn-Phong).
    // Exponent controls shininess; larger values yield a tighter highlight.
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 128.0);

    // Base lighting terms (diffuse scaled down for water).
    vec3 ambient   = uWaterColor * uAmbientColor * uAmbientIntensity;
    vec3 diffuse   = uWaterColor * uSunColor * uSunIntensity * (0.35 * NdotL);
    vec3 highlight =            uSunColor * uSunIntensity * (0.25 * spec);

    // Reflection sampling coordinates: project the current surface point into reflection texture space.
    vec2 uv = projectToUV(uViewRef, uProj, vWorldPos);

    // Vertical flip to match FBO texture coordinate convention (engine-dependent).
    uv.y = 1.0 - uv.y;

    // Small normal-based distortion (uses XZ components of world-space normal).
    uv += N.xz * uDistortStrength;

    // Clamp away from edges to reduce sampling artifacts outside the valid reflection image.
    uv = clamp(uv, vec2(0.001), vec2(0.999));

    vec3 refl = texture(uReflectTex, uv).rgb;

    vec3 base = ambient + diffuse + highlight;

    // Fresnel-driven mix: more reflective at grazing angles.
    vec3 color = mix(base, refl, clamp(fres * uReflectStrength, 0.0, 1.0));

    // Gamma correction to match other forward shaders.
    color = pow(color, vec3(1.0/2.2));
    FragColor = vec4(color, 1.0);
}
