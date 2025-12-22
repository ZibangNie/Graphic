#include "Sky.h"
#include <iostream>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

// 不用 TextureUtils.h：在这里声明 TextureUtils.cpp 里暴露出来的函数签名
namespace TextureUtils {
    GLuint LoadHDRTexture2D(const std::string& path);
    GLuint EquirectHDRToCubemap(GLuint hdrTex2D, int cubeSize,
                                const std::string& e2cVert,
                                const std::string& e2cFrag);
}

static float Smooth01(float x) {
    x = (x < 0.f) ? 0.f : (x > 1.f ? 1.f : x);
    return x * x * (3.f - 2.f * x);
}

void Sky::createCube() {
    // 36 vertices, positions only
    const float v[] = {
        -1,-1,-1,  1,-1,-1,  1, 1,-1,  1, 1,-1, -1, 1,-1, -1,-1,-1,
        -1,-1, 1,  1,-1, 1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1,-1, 1,
        -1, 1, 1, -1, 1,-1, -1,-1,-1, -1,-1,-1, -1,-1, 1, -1, 1, 1,
         1, 1, 1,  1, 1,-1,  1,-1,-1,  1,-1,-1,  1,-1, 1,  1, 1, 1,
        -1,-1,-1,  1,-1,-1,  1,-1, 1,  1,-1, 1, -1,-1, 1, -1,-1,-1,
        -1, 1,-1,  1, 1,-1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1, 1,-1
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glBindVertexArray(0);
}

bool Sky::init(const std::filesystem::path& assetsRoot,
               const std::string& dayHdrRel,
               const std::string& nightHdrRel,
               int cubemapSize) {

    shutdown();

    const auto e2cVert = (assetsRoot / "shaders/equirect2cube.vert").string();
    const auto e2cFrag = (assetsRoot / "shaders/equirect2cube.frag").string();
    const auto skyVert = (assetsRoot / "shaders/skybox.vert").string();
    const auto skyFrag = (assetsRoot / "shaders/skybox.frag").string();

    if (!m_shader.loadFromFiles(skyVert, skyFrag)) {
        std::cerr << "[Sky] skybox shader load failed.\n";
        return false;
    }

    // Load HDR
    const auto dayPath   = (assetsRoot / dayHdrRel).string();
    const auto nightPath = (assetsRoot / nightHdrRel).string();

    GLuint dayHdr = TextureUtils::LoadHDRTexture2D(dayPath);
    GLuint nightHdr = TextureUtils::LoadHDRTexture2D(nightPath);

    if (!dayHdr || !nightHdr) {
        std::cerr << "[Sky] HDR load failed. day=" << dayPath << " night=" << nightPath << "\n";
        if (dayHdr) glDeleteTextures(1, &dayHdr);
        if (nightHdr) glDeleteTextures(1, &nightHdr);
        return false;
    }

    // HDR -> Cubemap
    m_dayCube   = TextureUtils::EquirectHDRToCubemap(dayHdr, cubemapSize, e2cVert, e2cFrag);
    m_nightCube = TextureUtils::EquirectHDRToCubemap(nightHdr, cubemapSize, e2cVert, e2cFrag);

    glDeleteTextures(1, &dayHdr);
    glDeleteTextures(1, &nightHdr);

    if (!m_dayCube || !m_nightCube) {
        std::cerr << "[Sky] equirect->cubemap failed.\n";
        shutdown();
        return false;
    }

    createCube();

    m_ready = true;
    return true;
}

void Sky::render(const Camera& camera, const glm::mat4& proj, const Environment& env) {
    if (!m_ready) return;

    // view without translation
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 viewNoTrans = glm::mat4(glm::mat3(view));

    // day factor from sun height (你如果发现日夜反了，把 sunY 改成 -sunY)
    float sunY = env.sun().light().direction.y; // [-1,1]
    float day = Smooth01((sunY + 0.05f) / 0.35f); // 太阳略高于地平线就开始变亮

    float t = env.time().normalizedTime();
    float starRot = t * glm::two_pi<float>() * 0.15f; // 慢转：一天转 0.15 圈左右

    // 太阳在天空的方向：你现在 main 里 sunPos = pivot + sunDir * 120，说明 direction 是“指向太阳的位置”
    glm::vec3 sunDirSky = glm::normalize(env.sun().light().direction);

    // Draw skybox
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    m_shader.use();
    m_shader.setMat4("uProj", proj);
    m_shader.setMat4("uViewNoTrans", viewNoTrans);
    m_shader.setFloat("uDayFactor", day);
    m_shader.setFloat("uStarRot", starRot);
    m_shader.setVec3("uSunDir", sunDirSky);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_dayCube);
    m_shader.setInt("uSkyDay", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_nightCube);
    m_shader.setInt("uSkyNight", 1);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}

void Sky::shutdown() {
    if (m_dayCube) glDeleteTextures(1, &m_dayCube);
    if (m_nightCube) glDeleteTextures(1, &m_nightCube);
    m_dayCube = m_nightCube = 0;

    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    m_vbo = m_vao = 0;

    m_ready = false;
}
