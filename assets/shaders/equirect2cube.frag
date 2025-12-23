/*
 * equirect2cube.frag
 *
 * Purpose:
 *   Converts an equirectangular (lat-long) environment map into a cubemap face.
 *   Intended for offline/one-time rendering into a cubemap texture using a cube mesh.
 *
 * Inputs:
 *   - vLocalPos : Local-space position on the cube (interpreted as a direction vector).
 *
 * Uniforms:
 *   - uEquirect : 2D equirectangular HDR/LDR texture (longitude/latitude layout).
 *
 * Outputs:
 *   - FragColor : Sampled RGB from the equirectangular map (alpha = 1.0).
 *
 * Notes:
 *   - sampleSphericalMap() maps a normalized direction vector to equirectangular UV coordinates.
 *   - The Y flip (uv.y = 1 - uv.y) compensates for texture coordinate conventions so that the
 *     resulting cubemap has the expected vertical orientation.
 */

#version 330 core

out vec4 FragColor;

in vec3 vLocalPos;

uniform sampler2D uEquirect;

// Precomputed 1/(2*pi) and 1/pi to map atan/asin outputs into [0,1] UV space.
const vec2 invAtan = vec2(0.1591, 0.3183);

/*
 * Maps a world/local direction vector to equirectangular UV coordinates.
 *
 * Parameters:
 *   v : Normalized direction vector.
 *
 * Returns:
 *   uv in [0,1]^2 where:
 *     - u corresponds to longitude (atan(z, x)) wrapped into [0,1]
 *     - v corresponds to latitude (asin(y)) mapped into [0,1]
 */
vec2 sampleSphericalMap(vec3 v) {
    // v: normalized direction
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main() {
    // Interpret cube local position as a direction vector.
    vec3 dir = normalize(vLocalPos);

    // Convert direction to equirectangular UV.
    vec2 uv = sampleSphericalMap(dir);

    // Flip vertical axis to match the source texture's convention.
    uv.y = 1.0 - uv.y;

    // Sample the environment texture.
    vec3 color = texture(uEquirect, uv).rgb;
    FragColor = vec4(color, 1.0);
}
