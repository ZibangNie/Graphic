/*
 * Sky.h
 *
 * Purpose:
 *   Declares the Sky system responsible for:
 *     - Loading day and night HDR environment maps (equirectangular)
 *     - Converting them into cubemaps
 *     - Rendering a skybox with day/night blending and sun/star parameters (implemented in skybox shaders)
 *
 * Ownership:
 *   - Owns OpenGL cubemap textures for day and night.
 *   - Owns a cube mesh (VAO/VBO) used to render the skybox.
 *   - Owns the Shader program used for skybox rendering.
 *
 * Usage:
 *   - init(...) once at startup with the assets root and HDR paths.
 *   - render(...) once per frame with camera/projection and environment state.
 *   - shutdown() during teardown (safe to call multiple times).
 */

#pragma once
#include <string>
#include <filesystem>
#include <glad/glad.h>
#include <glm/glm.hpp>

#include "scene/Camera.h"
#include "Environment.h"
#include "render/Shader.h"

class Sky {
public:
    /*
     * Initializes skybox resources and loads environment maps.
     *
     * Parameters:
     *   assetsRoot   : Root directory for asset resolution (used to find shaders and textures).
     *   dayHdrRel    : Relative path from assetsRoot to the day HDR equirectangular image.
     *   nightHdrRel  : Relative path from assetsRoot to the night HDR equirectangular image.
     *   cubemapSize  : Cubemap face resolution in pixels (e.g., 256/512/1024). Higher values increase memory and conversion cost.
     *
     * Returns:
     *   true on success; false if shader loading, HDR loading, or equirect->cubemap conversion fails.
     */
    bool init(const std::filesystem::path& assetsRoot,
              const std::string& dayHdrRel,
              const std::string& nightHdrRel,
              int cubemapSize = 512);

    /*
     * Renders the skybox.
     *
     * Parameters:
     *   camera : Active camera; translation is removed internally so the skybox remains centered.
     *   proj   : Projection matrix for the current frame.
     *   env    : Environment state providing sun direction and normalized time for day/night blending and star rotation.
     *
     * Notes:
     *   - Temporarily adjusts depth state during drawing (implementation detail in Sky.cpp).
     */
    void render(const Camera& camera,
                const glm::mat4& proj,
                const Environment& env);

    /*
     * Releases OpenGL resources owned by Sky.
     *
     * Notes:
     *   - Safe to call multiple times; resources are deleted only if allocated.
     */
    void shutdown();

private:
    // Day/night cubemap textures (GL_TEXTURE_CUBE_MAP).
    GLuint m_dayCube = 0;
    GLuint m_nightCube = 0;

    // Cube mesh used to render the skybox.
    GLuint m_vao = 0;
    GLuint m_vbo = 0;

    Shader m_shader;
    bool m_ready = false;

    /*
     * Creates the cube geometry (VAO/VBO) for skybox rendering.
     */
    void createCube();
};
