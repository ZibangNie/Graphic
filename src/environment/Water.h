/*
 * Water.h
 *
 * Purpose:
 *   Declares the Water system that renders an animated water surface with planar reflections.
 *
 * Responsibilities:
 *   - Own and manage a reflection framebuffer (color texture + depth renderbuffer)
 *   - Provide a reflection render pass interface (beginReflectionPass / endReflectionPass)
 *   - Render the water mesh using the reflection texture and shared environment lighting uniforms
 *
 * Rendering workflow (typical frame):
 *   1) beginReflectionPass()
 *      - Render the scene from a mirrored/reflection camera into the internal reflection FBO.
 *   2) endReflectionPass(mainFbW, mainFbH)
 *      - Restore default framebuffer and viewport.
 *   3) render(...)
 *      - Draw the animated water plane, sampling the reflection texture.
 *
 * Notes:
 *   - Reflection is allocated at half the main framebuffer resolution for performance.
 *   - The reflection camera (viewRef) and any reflection clip plane logic are supplied externally.
 */

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
    /*
     * Initializes water resources.
     *
     * Parameters:
     *   assetsRoot : Root directory used to locate water shaders.
     *   fbW, fbH   : Main framebuffer dimensions in pixels (used to size the reflection buffer).
     *   waterY     : Base water level in world units.
     *   minX,maxX  : Water plane X extent in world units.
     *   minZ,maxZ  : Water plane Z extent in world units.
     *
     * Returns:
     *   true on success; false if shader loading fails.
     */
    bool init(const std::filesystem::path& assetsRoot,
              int fbW, int fbH,
              float waterY,
              float minX, float maxX,
              float minZ, float maxZ);

    /*
     * Releases reflection framebuffer resources owned by Water.
     *
     * Notes:
     *   - Safe to call multiple times.
     */
    void shutdown();

    /*
     * Resizes the reflection framebuffer to match the main framebuffer (at reduced resolution).
     *
     * Parameters:
     *   fbW, fbH : Main framebuffer dimensions in pixels.
     */
    void resize(int fbW, int fbH);

    /*
     * Begins the reflection rendering pass.
     *
     * Side effects:
     *   - Binds the internal reflection FBO.
     *   - Sets viewport to reflection resolution.
     *   - Clears color and depth buffers.
     */
    void beginReflectionPass();

    /*
     * Ends the reflection rendering pass and restores the main framebuffer viewport.
     *
     * Parameters:
     *   mainFbW, mainFbH : Main framebuffer dimensions in pixels.
     */
    void endReflectionPass(int mainFbW, int mainFbH);

    // Returns the reflection color attachment texture (GL_TEXTURE_2D).
    GLuint reflectTexture() const { return m_reflectColorTex; }

    /*
     * Renders the water surface (samples the reflection texture).
     *
     * Parameters:
     *   camera   : Active camera (used for camera-dependent lighting uniforms).
     *   view     : Main camera view matrix.
     *   proj     : Main camera projection matrix.
     *   viewRef  : Reflection camera view matrix (used to project world positions into reflection UVs).
     *   env      : Environment state (sun/ambient parameters).
     *   lighting : LightingSystem that applies shared environment lighting uniforms to the shader.
     *   timeSec  : Absolute time in seconds used for wave animation.
     *
     * Notes:
     *   - The water shaders typically expect the reflection texture to be bound at a fixed unit.
     */
    void render(const Camera& camera,
                const glm::mat4& view,
                const glm::mat4& proj,
                const glm::mat4& viewRef,
                const Environment& env,
                const LightingSystem& lighting,
                float timeSec);

    // Returns the base water level in world units.
    float waterY() const { return m_waterY; }

private:
    // Base water plane height in world units.
    float m_waterY = 0.0f;

    Mesh   m_mesh;
    Shader m_shader;

    // Reflection framebuffer resources.
    GLuint m_reflectFBO = 0;
    GLuint m_reflectColorTex = 0; // RGBA8 color attachment sampled during water rendering.
    GLuint m_reflectDepthRBO = 0; // Depth buffer used during reflection rendering.

    // Reflection framebuffer dimensions (typically half the main framebuffer).
    int m_fboW = 1;
    int m_fboH = 1;

private:
    /*
     * Allocates or resizes reflection framebuffer attachments based on main framebuffer size.
     *
     * Parameters:
     *   fbW, fbH : Main framebuffer dimensions in pixels.
     */
    void createOrResizeReflectionFBO(int fbW, int fbH);

    /*
     * Utility: creates a tessellated plane mesh with interleaved position/normal/UV attributes.
     *
     * Parameters:
     *   minX,maxX : Plane X extent.
     *   minZ,maxZ : Plane Z extent.
     *   segX,segZ : Subdivisions along X/Z (clamped in implementation).
     */
    static Mesh createPlaneMesh(float minX, float maxX, float minZ, float maxZ, int segX, int segZ);
};
