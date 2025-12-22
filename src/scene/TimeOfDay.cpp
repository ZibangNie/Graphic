#include "TimeOfDay.h"
#include <glm/gtc/constants.hpp>
#include <cmath>

static float saturate(float x){ return x<0?0:(x>1?1:x); }
static glm::vec3 mix(const glm::vec3& a, const glm::vec3& b, float t){ return a*(1-t)+b*t; }

LightingState ComputeLighting(float t01) {
    LightingState L;

    // 太阳角：让它绕 XZ 平面转一圈。你也可以换成绕 X 轴转，看你场景更像哪种。
    // 这里定义：t=0.5 正午（太阳最高），t=0 午夜（太阳最低）
    float theta = (t01 - 0.25f) * glm::two_pi<float>(); // t=0.25 -> sunrise 角度 0
    float sunY  = std::sin(theta);  // [-1,1]
    float sunX  = std::cos(theta);

    glm::vec3 sceneToSun = glm::normalize(glm::vec3(sunX, sunY, 0.35f));
    glm::vec3 sunToScene = -sceneToSun; // 统一：sun -> scene

    float day = saturate(sceneToSun.y * 0.7f + 0.3f);
    float horizon = 1.0f - saturate(std::abs(sceneToSun.y) * 3.0f);

    glm::vec3 noonColor   = glm::vec3(1.0f, 0.98f, 0.95f);
    glm::vec3 sunsetColor = glm::vec3(1.0f, 0.55f, 0.25f);
    glm::vec3 nightColor  = glm::vec3(0.4f, 0.55f, 0.9f);

    glm::vec3 sunColor = mix(noonColor, sunsetColor, horizon);
    sunColor = mix(nightColor, sunColor, day);

    float sunIntensity = 0.05f + 1.20f * day;
    float ambIntensity = 0.02f + 0.35f * day;
    glm::vec3 ambColor = mix(glm::vec3(0.03f,0.04f,0.07f), glm::vec3(0.35f,0.38f,0.40f), day);

    // 填充扁平 state
    L.time01 = t01;
    L.dayFactor = day;
    L.horizonFactor = horizon;

    L.sunDir = sunToScene;
    L.sunColor = sunColor;
    L.sunIntensity = sunIntensity;

    L.ambientColor = ambColor;
    L.ambientIntensity = ambIntensity;

    return L;
}
