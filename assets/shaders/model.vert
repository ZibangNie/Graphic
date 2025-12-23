/*
 * model.vert
 *
 * Purpose:
 *   Vertex shader for model geometry providing world-space position and UVs to the fragment shader.
 *   Applies model/view/projection transforms.
 *
 * Vertex Inputs:
 *   - aPos    : Object/local-space position.
 *   - aNormal : Object/local-space normal (currently unused in this shader stage; reserved for lighting pipelines).
 *   - aUV     : Texture coordinates for albedo/base color sampling.
 *
 * Uniforms:
 *   - uModel : Object/local -> world transform.
 *   - uView  : World -> view/camera transform.
 *   - uProj  : View -> clip (projection) transform.
 *
 * Varyings:
 *   - vWorldPos : World-space position, commonly used for clipping, lighting, and world-space effects.
 *   - vUV       : UV coordinates forwarded to fragment stage for texture sampling.
 *
 * Notes:
 *   - aNormal is accepted to match common mesh layouts; it is not forwarded or transformed here.
 *     If lighting is added, normals should be transformed with the inverse-transpose of uModel.
 */

#version 330 core

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vWorldPos;
out vec2 vUV;

void main() {
    // Transform vertex position to world space.
    vec4 wp = uModel * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;

    // Pass through texture coordinates.
    vUV = aUV;

    // Final clip-space position.
    gl_Position = uProj * uView * wp;
}
