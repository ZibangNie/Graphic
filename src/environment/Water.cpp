#include "environment/Water.h"

#include <iostream>
#include <vector>
#include <algorithm>

#include "scene/Camera.h"
#include "environment/Environment.h"
#include "render/LightingSystem.h"

static int clampi(int v, int lo, int hi) { return (v < lo) ? lo : (v > hi ? hi : v); }

Mesh Water::createPlaneMesh(float minX, float maxX, float minZ, float maxZ, int segX, int segZ)
{
    Mesh m;
    segX = clampi(segX, 1, 1024);
    segZ = clampi(segZ, 1, 1024);

    std::vector<float> v;
    v.reserve((size_t)segX * (size_t)segZ * 6ull * 8ull);

    auto push = [&](float x, float y, float z, float nx, float ny, float nz, float u, float t) {
        v.push_back(x); v.push_back(y); v.push_back(z);
        v.push_back(nx); v.push_back(ny); v.push_back(nz);
        v.push_back(u); v.push_back(t);
    };

    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };

    for (int z = 0; z < segZ; ++z) {
        float tz0 = (float)z / (float)segZ;
        float tz1 = (float)(z + 1) / (float)segZ;

        float z0 = lerp(minZ, maxZ, tz0);
        float z1 = lerp(minZ, maxZ, tz1);

        for (int x = 0; x < segX; ++x) {
            float tx0 = (float)x / (float)segX;
            float tx1 = (float)(x + 1) / (float)segX;

            float x0 = lerp(minX, maxX, tx0);
            float x1 = lerp(minX, maxX, tx1);

            // y 先给 0，真正高度在 vertex shader 里用 uWaterY 决定
            const float y = 0.0f;
            const float nx = 0.0f, ny = 1.0f, nz = 0.0f;

            // tri 1
            push(x0, y, z0, nx,ny,nz, tx0, tz0);
            push(x1, y, z0, nx,ny,nz, tx1, tz0);
            push(x1, y, z1, nx,ny,nz, tx1, tz1);

            // tri 2
            push(x0, y, z0, nx,ny,nz, tx0, tz0);
            push(x1, y, z1, nx,ny,nz, tx1, tz1);
            push(x0, y, z1, nx,ny,nz, tx0, tz1);
        }
    }

    m.uploadInterleavedPosNormalUV(v);
    return m;
}

void Water::createOrResizeReflectionFBO(int fbW, int fbH)
{
    // 性能：用半分辨率足够
    m_fboW = std::max(1, fbW / 2);
    m_fboH = std::max(1, fbH / 2);

    if (!m_reflectFBO) glGenFramebuffers(1, &m_reflectFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_reflectFBO);

    // color tex
    if (!m_reflectColorTex) glGenTextures(1, &m_reflectColorTex);
    glBindTexture(GL_TEXTURE_2D, m_reflectColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_fboW, m_fboH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_reflectColorTex, 0);

    // depth rbo
    if (!m_reflectDepthRBO) glGenRenderbuffers(1, &m_reflectDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_reflectDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_fboW, m_fboH);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_reflectDepthRBO);

    GLenum drawBufs[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBufs);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[Water] Reflection FBO incomplete. status=" << status << "\n";
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool Water::init(const std::filesystem::path& assetsRoot,
                 int fbW, int fbH,
                 float waterY,
                 float minX, float maxX,
                 float minZ, float maxZ)
{
    shutdown();

    m_waterY = waterY;

    // shader
    if (!m_shader.loadFromFiles((assetsRoot / "shaders/water.vert").string(),
                                (assetsRoot / "shaders/water.frag").string())) {
        std::cerr << "[Water] water shader load failed\n";
        return false;
    }

    // mesh（分段不要太离谱，避免太多三角形）
    m_mesh = createPlaneMesh(minX, maxX, minZ, maxZ, 220, 220);

    createOrResizeReflectionFBO(fbW, fbH);
    return true;
}

void Water::shutdown()
{
    if (m_reflectDepthRBO) glDeleteRenderbuffers(1, &m_reflectDepthRBO);
    if (m_reflectColorTex) glDeleteTextures(1, &m_reflectColorTex);
    if (m_reflectFBO) glDeleteFramebuffers(1, &m_reflectFBO);

    m_reflectDepthRBO = 0;
    m_reflectColorTex = 0;
    m_reflectFBO = 0;
}

void Water::resize(int fbW, int fbH)
{
    createOrResizeReflectionFBO(fbW, fbH);
}

void Water::beginReflectionPass()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_reflectFBO);
    glViewport(0, 0, m_fboW, m_fboH);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Water::endReflectionPass(int mainFbW, int mainFbH)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, mainFbW, mainFbH);
}

void Water::render(const Camera& camera,
                   const glm::mat4& view,
                   const glm::mat4& proj,
                   const glm::mat4& viewRef,
                   const Environment& env,
                   const LightingSystem& lighting,
                   float timeSec)
{
    // 让水面也吃到同一套太阳/环境光参数
    lighting.applyFromEnvironment(m_shader, camera, env);

    m_shader.use();
    m_shader.setMat4("uModel", glm::mat4(1.0f));
    m_shader.setMat4("uView", view);
    m_shader.setMat4("uProj", proj);
    m_shader.setMat4("uViewRef", viewRef);

    m_shader.setFloat("uTime", timeSec);
    m_shader.setFloat("uWaterY", m_waterY);

    // 参数可后续调
    m_shader.setVec3("uWaterColor", glm::vec3(0.02f, 0.15f, 0.22f));
    m_shader.setFloat("uReflectStrength", 1.0f);
    m_shader.setFloat("uDistortStrength", 0.02f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_reflectColorTex);
    m_shader.setInt("uReflectTex", 0);

    m_mesh.draw();
}
