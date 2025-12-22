#pragma once

#include "Shader.h"
#include "environment/Sun.h"   // DirectionalLight 定义在这里（按你工程实际路径）

// 用前置声明避免头文件互相包含
class Camera;
class Environment;

class LightingSystem {
public:
    // 最基础：直接把一束平行光写入 shader
    void applyDirectionalLight(Shader& shader, const DirectionalLight& light) const;

    // 新版：从 Environment 中取太阳光与时间，写入 shader
    void applyFromEnvironment(Shader& shader, const Camera& camera, const Environment& env) const;
};
