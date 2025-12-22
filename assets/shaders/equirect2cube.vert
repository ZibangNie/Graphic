#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 vLocalPos;

uniform mat4 uProj;
uniform mat4 uView;

void main() {
    vLocalPos = aPos;
    gl_Position = uProj * uView * vec4(aPos, 1.0);
}
