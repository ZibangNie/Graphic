#pragma once
#include <glm/glm.hpp>
#include "core/Input.h"
#include "render/Shader.h"
#include "scene/Camera.h"
#include "scene/TimeOfDay.h"

class LightingSystem {
public:
    void update(float dt, const Input& input);
    void applyTo(Shader& shader, const Camera& camera) const;

    const LightingState& state() const { return m_state; }

    float daySpeed = 0.02f;
    float time01   = 0.25f;

private:
    LightingState m_state{};
};
