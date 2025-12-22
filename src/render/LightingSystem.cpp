#include "LightingSystem.h"

#include "scene/Camera.h"
#include "environment/Environment.h"

#include <glm/glm.hpp>

void LightingSystem::applyDirectionalLight(Shader& shader, const DirectionalLight& light) const {
    shader.use();
    shader.setVec3("uSunDir", glm::normalize(light.direction));
    shader.setVec3("uSunColor", light.color);
    shader.setFloat("uSunIntensity", light.intensity);

    // 默认环境光，避免全黑
    shader.setVec3("uAmbientColor", glm::vec3(1.0f));
    shader.setFloat("uAmbientIntensity", 0.35f);
}

void LightingSystem::applyFromEnvironment(Shader& shader, const Camera& camera, const Environment& env) const {
    const DirectionalLight& L = env.sun().light();

    shader.use();
    shader.setVec3("uSunDir", glm::normalize(L.direction));
    shader.setVec3("uSunColor", L.color);
    shader.setFloat("uSunIntensity", L.intensity);

    shader.setVec3("uAmbientColor", glm::vec3(1.0f));
    float sunY = glm::normalize(L.direction).y;
    float day = glm::clamp((sunY - 0.02f) / 0.35f, 0.0f, 1.0f);
    day = day * day;

    // 夜晚 ambient 很低，白天也不要太高（否则永远“亮堂堂”）
    float ambIntensity = 0.01f + 0.20f * day;

    shader.setVec3("uAmbientColor", glm::vec3(1.0f));
    shader.setFloat("uAmbientIntensity", ambIntensity);

    shader.setVec3("uCameraPos", camera.position);

    // 只有当你的 shader 里真的声明了 uTimeOfDay01 才需要保留
    shader.setFloat("uTimeOfDay01", env.time().normalizedTime());
}
