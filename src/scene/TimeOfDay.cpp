#include "TimeOfDay.h"
#include <glm/gtc/constants.hpp>
#include <cmath>

static float saturate(float x){ return x<0?0:(x>1?1:x); }
static glm::vec3 mix(const glm::vec3& a, const glm::vec3& b, float t){ return a*(1-t)+b*t; }

LightingState ComputeLighting(float t01) {
    LightingState L;
    L.timeOfDay01 = t01;

    // 太阳角：让它绕 XZ 平面转一圈。你也可以换成绕 X 轴转，看你场景更像哪种。
    // 这里定义：t=0.5 正午（太阳最高），t=0 午夜（太阳最低）
    float theta = (t01 - 0.25f) * glm::two_pi<float>(); // t=0.25 -> sunrise 角度 0
    float sunY  = std::sin(theta);  // [-1,1]
    float sunX  = std::cos(theta);

    // 太阳方向：从“场景指向太阳”的方向（light direction），你 shader 里要统一使用方式
    glm::vec3 sunDir = glm::normalize(glm::vec3(sunX, sunY, 0.35f)); // z 给一点倾斜避免太平

    // 白天程度：太阳高度 >0 才算白天
    float day = saturate(sunDir.y * 0.7f + 0.3f); // 0..1

    // 日出日落暖色：太阳接近地平线时增强
    float horizon = 1.0f - saturate(std::abs(sunDir.y) * 3.0f); // y 越接近 0 越大

    glm::vec3 noonColor   = glm::vec3(1.0f, 0.98f, 0.95f);
    glm::vec3 sunsetColor = glm::vec3(1.0f, 0.55f, 0.25f);
    glm::vec3 nightColor  = glm::vec3(0.4f, 0.55f, 0.9f);

    glm::vec3 sunColor = mix(noonColor, sunsetColor, horizon);
    sunColor = mix(nightColor, sunColor, day);

    float sunIntensity = 0.05f + 1.20f * day;      // 夜晚很弱，白天强
    float ambIntensity = 0.02f + 0.35f * day;      // 夜晚更暗
    glm::vec3 ambColor = mix(glm::vec3(0.03f,0.04f,0.07f), glm::vec3(0.35f,0.38f,0.40f), day);

    L.sun.direction = sunDir;
    L.sun.color = sunColor;
    L.sun.intensity = sunIntensity;

    L.ambient.color = ambColor;
    L.ambient.intensity = ambIntensity;

    return L;
}
