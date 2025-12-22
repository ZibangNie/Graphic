#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormalPacked; // 原 aColor 现在存 normal

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform float uUVScale;

out vec2 vUV;
out float vH;
out vec3 vWorldPos;
out vec3 vNormalW;

void main() {
    vec3 worldPos = (uModel * vec4(aPos, 1.0)).xyz;
    vWorldPos = worldPos;

    // normal: object->world（你的地形 model 通常无非等比缩放，直接 mat3(uModel) 足够）
    vNormalW = normalize(mat3(uModel) * aNormalPacked);

    vUV = worldPos.xz * uUVScale;
    vH  = worldPos.y;

    gl_Position = uProj * uView * vec4(worldPos, 1.0);
}