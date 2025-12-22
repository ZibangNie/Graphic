#pragma once
#include <glm/glm.hpp>

struct SunLight {
    glm::vec3 direction;   // world space, pointing FROM fragment TO light? 统一口径见下
    glm::vec3 color;       // RGB 0..1
    float intensity;       // scalar
};

struct AmbientLight {
    glm::vec3 color;
    float intensity;
};

struct LightingState {
    float timeOfDay01 = 0.25f; // 0..1, 0=午夜, 0.25=日出, 0.5=正午, 0.75=日落
    SunLight sun;
    AmbientLight ambient;
};

LightingState ComputeLighting(float timeOfDay01);
