#pragma once
#include <glm/glm.hpp>

// 统一口径：sunDir = sun -> scene（光线传播方向/入射方向）
// 你的 shader 已验证：L = normalize(uSunDir) 才是正确方向
struct LightingState {
    float time01 = 0.25f;         // 0..1
    float dayFactor = 0.0f;       // 0..1 白天程度
    float horizonFactor = 0.0f;   // 0..1 日出日落程度

    glm::vec3 sunDir{0, -1, 0};   // sun -> scene
    glm::vec3 sunColor{1, 1, 1};
    float sunIntensity = 1.0f;

    glm::vec3 ambientColor{0.1f, 0.1f, 0.1f};
    float ambientIntensity = 0.2f;
};

LightingState ComputeLighting(float time01);
