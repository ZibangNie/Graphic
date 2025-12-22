#include "TimeOfDay.h"

void TimeOfDay::update(float dt) {
    constexpr float dayLengthSeconds = 30.0f; // 多少秒一昼夜
    m_time01 += dt / dayLengthSeconds;
    if (m_time01 > 1.0f)
        m_time01 -= 1.0f;
}
