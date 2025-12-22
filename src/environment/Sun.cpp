#include "Sun.h"
#include <glm/gtc/constants.hpp>
#include <cmath>

void Sun::update(const TimeOfDay& time) {
    float t = time.normalizedTime();

    // 太阳角度：早上从地平线升起
    float angle = (t - 0.25f) * glm::two_pi<float>();

    m_light.direction = glm::normalize(glm::vec3(
        std::cos(angle),
        std::sin(angle),
        std::sin(angle)
    ));

    float sunY = m_light.direction.y; // [-1,1]

    // day: 0(夜) -> 1(正午)，让过渡更陡一点，夜晚更暗
    float day = glm::clamp((sunY - 0.02f) / 0.35f, 0.0f, 1.0f);
    day = day * day; // 加强对比

    // horizon: 日出/日落更强，用于暖色
    float horizon = 1.0f - glm::clamp(std::abs(sunY) / 0.25f, 0.0f, 1.0f);
    horizon = horizon * horizon;

    glm::vec3 noon(1.0f, 0.97f, 0.92f);
    glm::vec3 dusk(1.0f, 0.45f, 0.20f);

    // 颜色：正午偏白，日出日落偏橙
    m_light.color = glm::mix(noon, dusk, 0.75f * horizon);

    // 强度：夜晚接近 0，白天显著更强
    m_light.intensity = 0.1f + 2.2f * day;

}
