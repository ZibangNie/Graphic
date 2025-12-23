/*
 * basic.vert
 *
 * Purpose:
 *   Vertex shader for vertex-colored geometry.
 *   Applies model/view/projection transforms, supports a global tint multiplier,
 *   and writes a user-defined clip distance for planar clipping.
 *
 * Vertex Inputs:
 *   - aPos   : Object/local-space position.
 *   - aColor : Per-vertex color (treated as linear albedo multiplier).
 *
 * Uniforms:
 *   - uModel : Object/local -> world transform.
 *   - uView  : World -> view/camera transform.
 *   - uProj  : View -> clip (projection) transform.
 *   - uTint  : Global RGB multiplier applied to aColor; typical range [0, 1] (can exceed for stylized looks).
 *   - uClipPlane : Clip plane in world space as vec4 (A,B,C,D) for plane equation:
 *                  A*x + B*y + C*z + D = 0.
 *                  Fragments with gl_ClipDistance[0] < 0 are clipped when clipping is enabled.
 *
 * Varyings:
 *   - vColor    : Interpolated vertex color after tinting.
 *   - vWorldPos : World-space position, used downstream for derivative-based normal reconstruction or effects.
 *
 * Notes:
 *   - gl_ClipDistance requires GL_CLIP_DISTANCEi to be enabled in the pipeline to take effect.
 */

#version 330 core

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

uniform vec3 uTint;
uniform vec4 uClipPlane;

out vec3 vColor;
out vec3 vWorldPos;   // World-space position passed to fragment stage.

void main() {
    // Transform to world space.
    vec4 wp = uModel * vec4(aPos, 1.0);

    // World-space planar clipping: dot(wp, plane) provides signed distance (up to scale of plane normal).
    gl_ClipDistance[0] = dot(wp, uClipPlane);

    // Pass world position for downstream shading/effects.
    vWorldPos = wp.xyz;

    // Apply global tint to per-vertex color.
    vColor = aColor * uTint;

    // Final clip-space position.
    gl_Position = uProj * uView * wp;
}
