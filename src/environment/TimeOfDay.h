#pragma once

class TimeOfDay {
public:
    void update(float dt);

    float normalizedTime() const { return m_time01; }
    float hours() const { return m_time01 * 24.0f; }

private:
    float m_time01 = 0.25f; // 默认早上
};
