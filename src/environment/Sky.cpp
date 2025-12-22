#include "Sky.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

// 简单平滑插值
static float Smooth01(float x) {
    x = std::clamp(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

void Sky::render(const Camera&, const Environment& env) {
    float t = env.time().normalizedTime(); // 0~1

    // 太阳高度（用方向的 y 分量近似日夜强度）
    float sunY = env.sun().light().direction.y;   // [-1,1]
    float day = Smooth01((sunY + 0.15f) / 0.6f);  // 大概映射到 [0,1]

    // 天空颜色：夜→黎明→白天（够用，后面你再做云雾/太阳盘）
    glm::vec3 night(0.02f, 0.03f, 0.06f);
    glm::vec3 dawn (0.85f, 0.45f, 0.20f);
    glm::vec3 noon (0.45f, 0.65f, 0.95f);

    // 日出日落区间（用 dayFactor 控制过渡）
    glm::vec3 sky = night * (1.0f - day) + noon * day;

    // 在接近日出/日落时加一点暖色（简单但有效）
    float warm = 1.0f - std::abs(day * 2.0f - 1.0f); // noon 最小，边缘最大
    warm = Smooth01(warm) * 0.6f;
    sky = glm::mix(sky, dawn, warm);

    glClearColor(sky.r, sky.g, sky.b, 1.0f);
}
