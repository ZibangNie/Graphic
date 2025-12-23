/*
 * Sky.cpp
 *
 * Purpose:
 *   Implements the Sky system responsible for:
 *     - Loading day and night HDR equirectangular environment maps
 *     - Converting them into cubemaps for efficient sampling
 *     - Rendering a skybox with a day/night blend and a procedural sun disk (implemented in skybox.frag)
 *
 * Rendering pipeline:
 *   - HDR equirectangular textures are loaded as 2D textures.
 *   - Each HDR is converted into a cubemap via an offscreen equirect->cubemap pass.
 *   - Skybox rendering uses a cube mesh and a view matrix with translation removed.
 *
 * Notes:
 *   - Texture conversion utilities are declared locally to avoid including TextureUtils.h here.
 *   - Depth state is adjusted during skybox draw: depth test uses GL_LEQUAL and depth writes are disabled,
 *     then restored after rendering.
 */

#include "Sky.h"
#include <iostream>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

// TextureUtils forward declarations (implemented in TextureUtils.cpp).
namespace TextureUtils {
    GLuint LoadHDRTexture2D(const std::string& path);
    GLuint EquirectHDRToCubemap(GLuint hdrTex2D, int cubeSize,
                                const std::string& e2cVert,
                                const std::string& e2cFrag);
}

/*
 * Smoothstep-like easing function mapping [0,1] -> [0,1] with zero derivatives at endpoints.
 *
 * Parameters:
 *   x : Input value. Values outside [0,1] are clamped.
 *
 * Returns:
 *   Smoothed value in [0,1].
 */
static float Smooth01(float x) {
    x = (x < 0.f) ? 0.f : (x > 1.f ? 1.f : x);
    return x * x * (3.f - 2.f * x);
}

/*
 * Creates the cube mesh used for skybox rendering.
 *
 * Implementation details:
 *   - 36 vertices (12 triangles) with positions only.
 *   - Stored in a VAO/VBO as a single attribute at location 0 (vec3 position).
 */
void Sky::createCube() {
    // 36 vertices, positions only.
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

    // Attribute 0: position (vec3).
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glBindVertexArray(0);
}

/*
 * Initializes the sky system:
 *   - Loads day/night HDR equirectangular textures
 *   - Converts each to a cubemap of the requested size
 *   - Loads the skybox shader program and creates the cube mesh
 *
 * Parameters:
 *   assetsRoot   : Root directory for assets (used to locate shaders and HDR textures).
 *   dayHdrRel    : Relative path (from assetsRoot) to the day HDR equirectangular image.
 *   nightHdrRel  : Relative path (from assetsRoot) to the night HDR equirectangular image.
 *   cubemapSize  : Output cubemap face resolution in pixels (e.g., 512, 1024).
 *
 * Returns:
 *   true on success; false if shader loading, HDR loading, or cubemap conversion fails.
 *
 * Side effects:
 *   - Allocates OpenGL texture objects for cubemaps and a VAO/VBO for the cube.
 *   - Sets m_ready to true on success.
 */
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

    // Load HDR equirectangular maps as 2D textures.
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

    // Convert HDR equirectangular textures into cubemaps for runtime sampling.
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

/*
 * Renders the skybox.
 *
 * Parameters:
 *   camera : Active camera; used to obtain view matrix. Translation is removed to keep the skybox centered.
 *   proj   : Projection matrix for the current frame.
 *   env    : Environment state providing sun direction and normalized time-of-day.
 *
 * Behavior:
 *   - Computes uDayFactor from sun height (Y component of sun direction).
 *   - Computes a slow starfield rotation from normalized time-of-day.
 *   - Binds day/night cubemaps and draws the cube with the skybox shader.
 *
 * Notes:
 *   - uDayFactor is derived from sunY with a small offset so dawn begins slightly above the horizon.
 *   - If day/night appear swapped, the sign convention for sun direction may differ; flipping sunY is a common fix.
 *   - Depth function is temporarily set to GL_LEQUAL and depth writes are disabled during the sky draw.
 */
void Sky::render(const Camera& camera, const glm::mat4& proj, const Environment& env) {
    if (!m_ready) return;

    // View matrix without translation: keeps skybox "infinitely far" while rotating with the camera.
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 viewNoTrans = glm::mat4(glm::mat3(view));

    // Day factor derived from sun elevation (Y component). Expected sunY in [-1, 1].
    // If day/night are inverted, a sign flip on sunY is a typical correction.
    float sunY = env.sun().light().direction.y; // [-1,1]
    float day = Smooth01((sunY + 0.05f) / 0.35f); // Starts brightening slightly above the horizon.

    // Starfield rotation: slow spin over the day based on normalized time.
    float t = env.time().normalizedTime();
    float starRot = t * glm::two_pi<float>() * 0.15f; // ~0.15 revolutions per day.

    // Sun direction for sky shading; must match the convention expected by skybox.frag (direction toward sun).
    glm::vec3 sunDirSky = glm::normalize(env.sun().light().direction);

    // Draw skybox with relaxed depth test and disabled depth writes.
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    m_shader.use();
    m_shader.setMat4("uProj", proj);
    m_shader.setMat4("uViewNoTrans", viewNoTrans);
    m_shader.setFloat("uDayFactor", day);
    m_shader.setFloat("uStarRot", starRot);
    m_shader.setVec3("uSunDir", sunDirSky);

    // Bind cubemaps to fixed texture units.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_dayCube);
    m_shader.setInt("uSkyDay", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_nightCube);
    m_shader.setInt("uSkyNight", 1);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    // Restore depth state.
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}

/*
 * Releases sky resources.
 *
 * Side effects:
 *   - Deletes day/night cubemap textures if allocated.
 *   - Deletes the skybox cube VAO/VBO if allocated.
 *   - Marks the system as not ready.
 */
void Sky::shutdown() {
    if (m_dayCube) glDeleteTextures(1, &m_dayCube);
    if (m_nightCube) glDeleteTextures(1, &m_nightCube);
    m_dayCube = m_nightCube = 0;

    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    m_vbo = m_vao = 0;

    m_ready = false;
}
