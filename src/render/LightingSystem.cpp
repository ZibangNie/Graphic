#include "LightingSystem.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <algorithm>

void LightingSystem::update(float dt, const Input& input) {
    // 调速（你也可以把键位挪到外面）
    if (input.keyDown(GLFW_KEY_T)) daySpeed += 0.05f * dt;
    if (input.keyDown(GLFW_KEY_G)) daySpeed -= 0.05f * dt;
    daySpeed = std::clamp(daySpeed, 0.0f, 0.5f);

    time01 += daySpeed * dt;
    time01 = std::fmod(time01, 1.0f);
    if (time01 < 0.0f) time01 += 1.0f;

    m_state = ComputeLighting(time01);
    m_state.time01 = time01;

}

void LightingSystem::applyTo(Shader& shader, const Camera& camera) const {
    shader.use();
    shader.setVec3("uSunDir", m_state.sunDir);
    shader.setVec3("uSunColor", m_state.sunColor);
    shader.setFloat("uSunIntensity", m_state.sunIntensity);
    shader.setVec3("uAmbientColor", m_state.ambientColor);
    shader.setFloat("uAmbientIntensity", m_state.ambientIntensity);
    shader.setVec3("uCameraPos", camera.position);

    // 预留：天空/粒子/雾会用
    shader.setFloat("uTimeOfDay01", m_state.time01);
    shader.setFloat("uDayFactor", m_state.dayFactor);
    shader.setFloat("uHorizonFactor", m_state.horizonFactor);
}
