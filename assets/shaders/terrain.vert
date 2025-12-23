/*
 * terrain.vert
 *
 * Purpose:
 *   Vertex shader for terrain meshes.
 *   Computes world-space position and normal, generates planar UVs from world XZ, outputs height for
 *   material blending, and emits a clip distance for planar clipping.
 *
 * Vertex Inputs:
 *   - aPos          : Object/local-space position.
 *   - aNormalPacked : Object/local-space normal (stored in the former color attribute slot).
 *
 * Uniforms:
 *   - uModel    : Object/local -> world transform.
 *   - uView     : World -> view/camera transform.
 *   - uProj     : View -> clip (projection) transform.
 *   - uUVScale  : Scaling factor for planar UV generation from world XZ.
 *                Typical usage: smaller values => larger texture tiling; larger values => more repeats.
 *   - uClipPlane: Clip plane in world space as vec4 (A,B,C,D) for plane equation:
 *                A*x + B*y + C*z + D = 0.
 *                Vertices/fragments with gl_ClipDistance[0] < 0 are clipped when enabled.
 *
 * Varyings:
 *   - vUV       : Planar UVs derived from world XZ.
 *   - vH        : World-space height (worldPos.y), used for height-based blending in the fragment shader.
 *   - vWorldPos : World-space position (for effects such as clipping/fog).
 *   - vNormalW  : World-space normal (normalized).
 *
 * Notes:
 *   - Normal transform uses mat3(uModel), which is only strictly correct for rigid transforms or uniform scaling.
 *     For non-uniform scaling, normals should be transformed by the inverse-transpose of the model matrix.
 *   - gl_ClipDistance requires GL_CLIP_DISTANCE0 to be enabled to take effect.
 */

#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormalPacked; // Normal stored in former color attribute.

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform float uUVScale;
uniform vec4 uClipPlane; // Ax+By+Cz+D, <0 => clipped when enabled.

out vec2 vUV;
out float vH;
out vec3 vWorldPos;
out vec3 vNormalW;

void main() {
    // Compute world-space position.
    vec3 worldPos = (uModel * vec4(aPos, 1.0)).xyz;
    vWorldPos = worldPos;

    // World-space planar clipping.
    gl_ClipDistance[0] = dot(vec4(worldPos, 1.0), uClipPlane);

    // Transform normal into world space.
    // Assumes uModel is rigid or uniformly scaled; otherwise use inverse-transpose for correctness.
    vNormalW = normalize(mat3(uModel) * aNormalPacked);

    // Planar UV mapping using world XZ; uUVScale controls tiling frequency.
    vUV = worldPos.xz * uUVScale;

    // Height value for material blending (sand/rock thresholding).
    vH  = worldPos.y;

    // Final clip-space position.
    gl_Position = uProj * uView * vec4(worldPos, 1.0);
}
