/*
 * particle.vert
 *
 * Purpose:
 *   Vertex shader for a GPU instanced billboard particle system.
 *   Expands a 2D unit quad into a camera-facing quad in world space using camera right/up vectors,
 *   then projects to clip space. Also forwards per-instance attributes (age/seed/kind) to the fragment stage
 *   and emits a clip distance for planar clipping.
 *
 * Vertex Inputs (per-vertex):
 *   - aPos : 2D quad corner position in local billboard space.
 *            Expected range roughly [-0.5, 0.5] for each axis (unit quad centered at origin).
 *   - aUV  : UV coordinates corresponding to aPos (typically [0,1]).
 *
 * Instance Inputs (per-instance):
 *   - iPosSize      : xyz = particle center position in world space, w = particle size (world units).
 *   - iAgeSeedKind  : x = normalized age in [0,1], y = random seed in [0,1], z = kind selector.
 *                    Kind conventions are consumed in particle.frag (e.g., 0 flame, 1 ember, 2 glow).
 *
 * Uniforms:
 *   - uView      : World -> view/camera transform.
 *   - uProj      : View -> clip (projection) transform.
 *   - uCamRight  : Camera right unit vector in world space (should be normalized).
 *   - uCamUp     : Camera up unit vector in world space (should be normalized).
 *   - uClipPlane : Clip plane in world space as vec4 (A,B,C,D) for plane equation:
 *                  A*x + B*y + C*z + D = 0.
 *
 * Varyings:
 *   - vUV   : UV forwarded to fragment shader.
 *   - vAge  : Normalized age forwarded to fragment shader.
 *   - vSeed : Random seed forwarded to fragment shader.
 *   - vKind : Kind selector forwarded to fragment shader.
 *
 * Notes:
 *   - Billboarding uses uCamRight/uCamUp; this is "screen-facing" (axial) billboarding and assumes
 *     the camera basis vectors are consistent with uView.
 *   - gl_ClipDistance requires GL_CLIP_DISTANCE0 to be enabled to take effect.
 */

#version 330 core

layout (location = 0) in vec2 aPos;     // Billboard quad corner in local space (typically [-0.5, 0.5]).
layout (location = 1) in vec2 aUV;

layout (location = 2) in vec4 iPosSize;       // xyz = world position, w = size (world units).
layout (location = 3) in vec4 iAgeSeedKind;   // x = age01, y = seed, z = kind.

uniform mat4 uView;
uniform mat4 uProj;

uniform vec3 uCamRight; // World-space camera right vector (unit length recommended).
uniform vec3 uCamUp;    // World-space camera up vector (unit length recommended).

// Clip plane in world space (A,B,C,D).
uniform vec4 uClipPlane;

out vec2 vUV;
out float vAge;
out float vSeed;
out float vKind;

void main() {
    // Expand the unit quad into world space using camera basis vectors.
    // aPos is scaled by particle size (iPosSize.w) in world units.
    vec3 worldPos = iPosSize.xyz + (uCamRight * aPos.x + uCamUp * aPos.y) * iPosSize.w;

    // Transform into clip space.
    gl_Position = uProj * uView * vec4(worldPos, 1.0);

    // Pass attributes to fragment stage.
    vUV = aUV;
    vAge = iAgeSeedKind.x;
    vSeed = iAgeSeedKind.y;
    vKind = iAgeSeedKind.z;

    // World-space planar clipping; negative values are clipped when enabled.
    gl_ClipDistance[0] = dot(vec4(worldPos, 1.0), uClipPlane);
}
