#pragma once
#include <glm/glm.hpp>
#include "TimeOfDay.h"

struct DirectionalLight {
    glm::vec3 direction;
    glm::vec3 color;
    float intensity;
};

class Sun {
public:
    void update(const TimeOfDay& time);
    const DirectionalLight& light() const { return m_light; }

private:
    DirectionalLight m_light;
};
