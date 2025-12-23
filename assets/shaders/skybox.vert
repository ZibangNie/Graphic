/*
 * skybox.vert
 *
 * Purpose:
 *   Vertex shader for rendering a cubemap skybox.
 *   Removes camera translation so the skybox appears infinitely far away, and forces depth to the far plane
 *   to prevent the sky from being occluded by scene geometry during depth testing.
 *
 * Vertex Inputs:
 *   - aPos : Cube vertex position in local/object space (typically in [-1, 1] for each axis).
 *
 * Uniforms:
 *   - uProj       : Projection matrix.
 *   - uViewNoTrans: View matrix with translation removed, typically constructed as mat4(mat3(view)).
 *                  This keeps only camera rotation so the skybox follows camera orientation but not position.
 *
 * Varyings:
 *   - vDir : Direction vector used for cubemap sampling in the fragment shader.
 *
 * Notes:
 *   - gl_Position = pos.xyww sets z = w, resulting in normalized device depth of 1.0 (far plane) after division.
 *     This is a common technique to keep the skybox at maximum depth.
 */

#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 vDir;

uniform mat4 uProj;
uniform mat4 uViewNoTrans; // View matrix without translation: mat4(mat3(view)).

void main() {
    // Pass local position as sampling direction for the cubemap.
    vDir = aPos;

    // Standard clip-space transform using rotation-only view.
    vec4 pos = uProj * uViewNoTrans * vec4(aPos, 1.0);

    // Force depth to the far plane (z = w) so the skybox is not occluded by scene depth.
    gl_Position = pos.xyww;
}
