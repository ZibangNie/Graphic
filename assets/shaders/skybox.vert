#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 vDir;

uniform mat4 uProj;
uniform mat4 uViewNoTrans; // mat4(mat3(view))

void main() {
    vDir = aPos;
    vec4 pos = uProj * uViewNoTrans * vec4(aPos, 1.0);

    // 让深度固定到最远处，避免天空被深度测试挡掉
    gl_Position = pos.xyww;
}
