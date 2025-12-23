/*
 * basic.frag
 *
 * Purpose:
 *   Fragment shader for vertex-colored geometry with a minimal sun + ambient lighting model.
 *   Supports a simple emissive override for unlit rendering (e.g., debug objects, stylized glow).
 *
 * Inputs:
 *   - vColor     : Interpolated per-vertex color (treated as linear albedo).
 *   - vWorldPos  : Fragment position in world space (used to derive a geometric normal via screen-space derivatives).
 *
 * Outputs:
 *   - FragColor  : Final RGBA color in linear space (alpha = 1.0).
 *
 * Notes:
 *   - Normal is reconstructed from dFdx/dFdy of world position. This is suitable for procedurally generated
 *     surfaces or meshes without explicit normals, but can be sensitive to discontinuities and non-uniform
 *     interpolation across triangles.
 *   - Lighting is purely Lambertian (diffuse) + constant ambient. No specular term is included.
 *   - All colors/intensities are assumed to be in consistent (typically linear) space.
 */

#version 330 core

in vec3 vColor;
in vec3 vWorldPos;     // World-space position for derivative-based normal reconstruction.
out vec4 FragColor;

// Sun / key light direction and radiance controls.
// uSunDir        : Direction vector in world space; should be normalized or will be normalized here.
// uSunColor      : RGB light color (linear).
// uSunIntensity  : Scalar multiplier; typical range ~[0, 10] depending on scene exposure.
uniform vec3  uSunDir;
uniform vec3  uSunColor;
uniform float uSunIntensity;

// Ambient (constant) lighting term.
// uAmbientColor      : RGB ambient color (linear).
// uAmbientIntensity  : Scalar multiplier; typical range ~[0, 1] (can exceed for stylized looks).
uniform vec3  uAmbientColor;
uniform float uAmbientIntensity;

// Camera position in world space (reserved for extensions such as specular; unused in this shader).
uniform vec3  uCameraPos;

// Emissive override tint.
// uTint : RGB color used when emissive override is enabled (linear).
uniform vec3 uTint;

// Emissive mode flag.
// uEmissive : 0 = lit shading, 1 = emissive (unlit) output using uTint.
uniform int uEmissive; // 0/1

void main() {
    // Emissive path: bypasses all lighting and outputs a constant tint.
    if (uEmissive == 1) {
        FragColor = vec4(uTint, 1.0);
        return;
    }

    // Base surface color (linear albedo).
    vec3 albedo = vColor;

    // Screen-space derivatives of world position approximate local surface tangents.
    // Cross product yields a geometric normal in world space.
    vec3 dpdx = dFdx(vWorldPos);
    vec3 dpdy = dFdy(vWorldPos);
    vec3 N = normalize(cross(dpdx, dpdy)); // World-space normal; orientation depends on triangle winding.

    // Light direction in world space.
    vec3 L = normalize(uSunDir);

    // Lambertian cosine term; clamped to [0, 1] to avoid negative contribution on back-facing fragments.
    float NdotL = max(dot(N, L), 0.0);

    // Ambient term: constant illumination scaled by albedo.
    vec3 ambient = albedo * uAmbientColor * uAmbientIntensity;

    // Diffuse term: Lambertian response scaled by light color and intensity.
    vec3 diffuse = albedo * uSunColor * uSunIntensity * NdotL;

    // Final linear color.
    vec3 color = ambient + diffuse;
    FragColor = vec4(color, 1.0);
}
