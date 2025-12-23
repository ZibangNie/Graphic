#version 330 core

layout (location = 0) in vec2 aPos;     // [-0.5, 0.5] quad
layout (location = 1) in vec2 aUV;

layout (location = 2) in vec4 iPosSize;       // xyz = world pos, w = size
layout (location = 3) in vec4 iAgeSeedKind;   // x = age01, y = seed, z = kind

uniform mat4 uView;
uniform mat4 uProj;

uniform vec3 uCamRight;
uniform vec3 uCamUp;

// Same plane convention as your other shaders
uniform vec4 uClipPlane;

out vec2 vUV;
out float vAge;
out float vSeed;
out float vKind;

void main() {
    vec3 worldPos = iPosSize.xyz + (uCamRight * aPos.x + uCamUp * aPos.y) * iPosSize.w;

    gl_Position = uProj * uView * vec4(worldPos, 1.0);

    vUV = aUV;
    vAge = iAgeSeedKind.x;
    vSeed = iAgeSeedKind.y;
    vKind = iAgeSeedKind.z;

    gl_ClipDistance[0] = dot(vec4(worldPos, 1.0), uClipPlane);
}
