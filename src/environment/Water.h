#pragma once
#include <filesystem>
#include <glad/glad.h>
#include <glm/glm.hpp>

#include "render/Mesh.h"
#include "render/Shader.h"

class Camera;
class Environment;
class LightingSystem;

class Water {
public:
    bool init(const std::filesystem::path& assetsRoot,
              int fbW, int fbH,
              float waterY,
              float minX, float maxX,
              float minZ, float maxZ);

    void shutdown();

    void resize(int fbW, int fbH);

    void beginReflectionPass();
    void endReflectionPass(int mainFbW, int mainFbH);

    GLuint reflectTexture() const { return m_reflectColorTex; }

    // 绘制水面（采样反射贴图）
    void render(const Camera& camera,
                const glm::mat4& view,
                const glm::mat4& proj,
                const glm::mat4& viewRef,
                const Environment& env,
                const LightingSystem& lighting,
                float timeSec);

    float waterY() const { return m_waterY; }

private:
    float m_waterY = 0.0f;

    Mesh   m_mesh;
    Shader m_shader;

    // reflection FBO
    GLuint m_reflectFBO = 0;
    GLuint m_reflectColorTex = 0;
    GLuint m_reflectDepthRBO = 0;

    int m_fboW = 1;
    int m_fboH = 1;

private:
    void createOrResizeReflectionFBO(int fbW, int fbH);
    static Mesh createPlaneMesh(float minX, float maxX, float minZ, float maxZ, int segX, int segZ);
};
