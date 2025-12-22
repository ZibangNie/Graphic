#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

uniform vec3 uTint;

out vec3 vColor;
out vec3 vWorldPos;   // NEW

void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;                 // NEW
    vColor = aColor * uTint;
    gl_Position = uProj * uView * wp;
}
