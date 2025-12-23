/*
 * equirect2cube.vert
 *
 * Purpose:
 *   Vertex shader for rendering a cube when converting an equirectangular environment map
 *   into a cubemap. Passes the local-space vertex position as a direction basis to the
 *   fragment shader.
 *
 * Vertex Inputs:
 *   - aPos : Cube vertex position in local/object space. Typically in [-1, 1] for each axis.
 *
 * Uniforms:
 *   - uView : View matrix used to orient the cube face camera. Often constructed per cubemap face.
 *   - uProj : Projection matrix (commonly 90° FOV perspective) matching cubemap capture setup.
 *
 * Varyings:
 *   - vLocalPos : Local-space position forwarded to fragment shader and interpreted as a direction.
 *
 * Notes:
 *   - No model matrix is applied; the cube is assumed to be centered at the origin in capture space.
 *   - For cubemap capture, translation is typically removed from uView so only rotation affects sampling.
 */

#version 330 core

layout (location = 0) in vec3 aPos;

out vec3 vLocalPos;

uniform mat4 uProj;
uniform mat4 uView;

void main() {
    // Local-space position doubles as a direction vector for spherical mapping.
    vLocalPos = aPos;

    // Standard transform into clip space for the capture camera.
    gl_Position = uProj * uView * vec4(aPos, 1.0);
}
