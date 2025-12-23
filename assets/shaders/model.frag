/*
 * model.frag
 *
 * Purpose:
 *   Fragment shader for textured model rendering (base color only).
 *   Applies an optional albedo texture and a base color factor, with a world-space clip plane.
 *
 * Inputs:
 *   - vWorldPos : Fragment position in world space (used for planar clipping).
 *   - vUV       : Texture coordinates for base color (albedo) sampling.
 *
 * Uniforms:
 *   - uAlbedo          : Base color (albedo) texture sampler (2D).
 *   - uHasAlbedo       : Texture presence flag (0 = no texture, 1 = sample uAlbedo).
 *   - uBaseColorFactor : RGBA factor multiplied with the sampled/base color.
 *                        Typical channel ranges [0,1]; alpha is not used for output in this shader.
 *   - uClipPlane       : Clip plane in world space as vec4 (A,B,C,D) for plane equation:
 *                        A*x + B*y + C*z + D = 0.
 *
 * Outputs:
 *   - FragColor : Final base color (RGB from factor * optional texture), alpha forced to 1.0.
 *
 * Notes:
 *   - This shader performs manual clipping via discard instead of gl_ClipDistance.
 *   - No lighting is applied here; the result is unlit base color suitable for a later lighting pass
 *     or for stylized/unlit rendering.
 */

#version 330 core

in vec3 vWorldPos;
in vec2 vUV;

uniform sampler2D uAlbedo;
uniform int uHasAlbedo;               // 0/1
uniform vec4 uBaseColorFactor;        // Fallback base color and multiplier for texture.

uniform vec4 uClipPlane;

out vec4 FragColor;

void main() {
    // World-space planar clipping:
    //   dot(vec4(worldPos,1), plane) < 0 => discard (clip against the negative half-space).
    if (dot(vec4(vWorldPos, 1.0), uClipPlane) < 0.0) discard;

    // Base color starts from factor; multiply by albedo texture if present.
    vec4 c = uBaseColorFactor;
    if (uHasAlbedo == 1) c *= texture(uAlbedo, vUV);

    // Alpha is forced to 1.0 (opaque); c.a is ignored.
    FragColor = vec4(c.rgb, 1.0);
}
