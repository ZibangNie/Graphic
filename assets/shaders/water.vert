/*
 * water.vert
 *
 * Purpose:
 *   Vertex shader for an animated water surface.
 *   Builds a horizontal plane at uWaterY, applies procedural wave displacement in world space,
 *   computes a wave-derived normal, transforms outputs to world space, and projects to clip space.
 *
 * Vertex Inputs:
 *   - aPos    : Object/local-space position. Only x and z are used; y is overridden to 0 before uWaterY is applied.
 *   - aNormal : Provided normal (commonly (0,1,0)); not used because normals are derived from the wave function.
 *   - aUV     : Provided UVs; not used in this shader stage (reserved for future texturing).
 *
 * Uniforms:
 *   - uModel  : Object/local -> world transform for the water mesh.
 *   - uView   : World -> view/camera transform.
 *   - uProj   : View -> clip (projection) transform.
 *   - uTime   : Time in seconds used to animate waves.
 *   - uWaterY : Base water height in world units (sets the plane level before adding wave displacement).
 *
 * Varyings:
 *   - vWorldPos : Displaced world-space position of the water surface.
 *   - vNormalW  : World-space normal derived from wave slope.
 *
 * Notes:
 *   - waveHeight() implements a small sum of directional sine waves.
 *     Parameters:
 *       - k: spatial frequency (radians per world unit)
 *       - s: temporal speed multiplier
 *       - a: amplitude (world units)
 *   - dHdXZ returns partial derivatives (dH/dx, dH/dz) in world space, used to build the normal.
 *   - Normal transform uses inverse-transpose of uModel to remain correct under non-uniform scaling.
 */

#version 330 core
layout(location=0) in vec3 aPos;     // Plane position; y is overridden to 0 then set by uWaterY.
layout(location=1) in vec3 aNormal;  // Typically (0,1,0); unused (wave-derived normal is computed instead).
layout(location=2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

uniform float uTime;
uniform float uWaterY;

out vec3 vWorldPos;
out vec3 vNormalW;

/*
 * Computes procedural wave height at a world-space XZ position and its slope derivatives.
 *
 * Parameters:
 *   p     : World-space position in the water plane (xz).
 *   t     : Time in seconds.
 *   dHdXZ : Output partial derivatives of height with respect to (x, z):
 *           dHdXZ.x = dH/dx, dHdXZ.y = dH/dz
 *
 * Returns:
 *   Height offset in world units to be added to the base water level.
 */
float waveHeight(vec2 p, float t, out vec2 dHdXZ)
{
    float h = 0.0;
    dHdXZ = vec2(0.0);

    // Directional wave 1.
    // k = spatial frequency, s = speed, a = amplitude.
    // For large world coordinates, avoid very high k to reduce aliasing.
    vec2 d1 = normalize(vec2(1.0, 0.3));
    float k1 = 0.10;
    float s1 = 0.80;
    float a1 = 0.10;
    float ph1 = dot(d1, p) * k1 + t * s1;
    h += a1 * sin(ph1);
    dHdXZ += (a1 * cos(ph1) * k1) * d1;

    // Directional wave 2.
    vec2 d2 = normalize(vec2(-0.4, 1.0));
    float k2 = 0.16;
    float s2 = 1.10;
    float a2 = 0.06;
    float ph2 = dot(d2, p) * k2 + t * s2;
    h += a2 * sin(ph2);
    dHdXZ += (a2 * cos(ph2) * k2) * d2;

    // Directional wave 3.
    vec2 d3 = normalize(vec2(0.7, -0.9));
    float k3 = 0.22;
    float s3 = 1.60;
    float a3 = 0.03;
    float ph3 = dot(d3, p) * k3 + t * s3;
    h += a3 * sin(ph3);
    dHdXZ += (a3 * cos(ph3) * k3) * d3;

    return h;
}

void main()
{
    // Construct the base water plane in world space (y is controlled by uWaterY).
    vec4 wp = uModel * vec4(aPos.x, 0.0, aPos.z, 1.0);
    wp.y = uWaterY;

    // Apply wave displacement using world-space XZ coordinates.
    vec2 dHdXZ;
    float h = waveHeight(wp.xz, uTime, dHdXZ);
    wp.y += h;

    // Build the geometric normal from the heightfield slope: (-dH/dx, 1, -dH/dz).
    vec3 N = normalize(vec3(-dHdXZ.x, 1.0, -dHdXZ.y));

    vWorldPos = wp.xyz;

    // Transform normal into world space using inverse-transpose of the model matrix.
    vNormalW = normalize(mat3(transpose(inverse(uModel))) * N);

    // Final clip-space position.
    gl_Position = uProj * uView * wp;
}
