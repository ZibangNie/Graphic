/*
 * Water.cpp
 *
 * Purpose:
 *   Implements the Water system:
 *     - Builds a tessellated water plane mesh
 *     - Manages an offscreen reflection framebuffer (color + depth)
 *     - Provides a reflection render pass setup (begin/end)
 *     - Renders the animated water surface using shared environment lighting parameters
 *
 * Approach:
 *   - Reflection is rendered into an internal FBO at reduced resolution (half-size) for performance.
 *   - The water vertex shader applies procedural wave displacement and computes normals.
 *   - The water fragment shader combines base lighting, specular, and projected reflection with Fresnel blending.
 *
 * Notes:
 *   - The reflection camera view matrix (viewRef) is provided externally (typically mirrored about the water plane).
 *   - resize() must be called when the main framebuffer size changes to keep reflection resolution consistent.
 */

#include "environment/Water.h"

#include <iostream>
#include <vector>
#include <algorithm>

#include "scene/Camera.h"
#include "environment/Environment.h"
#include "render/LightingSystem.h"

// Integer clamp helper used to bound mesh subdivisions.
static int clampi(int v, int lo, int hi) { return (v < lo) ? lo : (v > hi ? hi : v); }

/*
 * Creates a tessellated plane mesh covering [minX,maxX] x [minZ,maxZ] in object/world space.
 *
 * Parameters:
 *   minX, maxX : Plane extent along X in world units.
 *   minZ, maxZ : Plane extent along Z in world units.
 *   segX       : Number of subdivisions along X (clamped to [1,1024]).
 *   segZ       : Number of subdivisions along Z (clamped to [1,1024]).
 *
 * Returns:
 *   Mesh containing interleaved position/normal/UV attributes suitable for the water shaders.
 *
 * Notes:
 *   - Vertex y is initialized to 0.0. The final water height is determined in the vertex shader via uWaterY
 *     plus procedural wave displacement.
 *   - Normals are initialized to (0,1,0); the vertex shader recomputes normals from wave derivatives.
 *   - UVs are generated as normalized grid coordinates in [0,1] across the plane.
 */
Mesh Water::createPlaneMesh(float minX, float maxX, float minZ, float maxZ, int segX, int segZ)
{
    Mesh m;
    segX = clampi(segX, 1, 1024);
    segZ = clampi(segZ, 1, 1024);

    std::vector<float> v;
    v.reserve((size_t)segX * (size_t)segZ * 6ull * 8ull);

    // Push one vertex with interleaved attributes:
    //   position (x,y,z), normal (nx,ny,nz), uv (u,t).
    auto push = [&](float x, float y, float z, float nx, float ny, float nz, float u, float t) {
        v.push_back(x); v.push_back(y); v.push_back(z);
        v.push_back(nx); v.push_back(ny); v.push_back(nz);
        v.push_back(u); v.push_back(t);
    };

    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };

    // Build grid cells; each cell contributes two triangles (6 vertices).
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

            // y is initialized to 0; uWaterY in the vertex shader sets the base plane level.
            const float y = 0.0f;
            const float nx = 0.0f, ny = 1.0f, nz = 0.0f;

            // Triangle 1
            push(x0, y, z0, nx,ny,nz, tx0, tz0);
            push(x1, y, z0, nx,ny,nz, tx1, tz0);
            push(x1, y, z1, nx,ny,nz, tx1, tz1);

            // Triangle 2
            push(x0, y, z0, nx,ny,nz, tx0, tz0);
            push(x1, y, z1, nx,ny,nz, tx1, tz1);
            push(x0, y, z1, nx,ny,nz, tx0, tz1);
        }
    }

    // Upload as an interleaved vertex buffer matching the Mesh's expected layout.
    m.uploadInterleavedPosNormalUV(v);
    return m;
}

/*
 * Creates or resizes the reflection framebuffer and its attachments.
 *
 * Parameters:
 *   fbW, fbH : Main framebuffer dimensions in pixels. Reflection is allocated at half resolution.
 *
 * Behavior:
 *   - Allocates/updates:
 *       - m_reflectFBO (framebuffer object)
 *       - m_reflectColorTex (RGBA8 color attachment)
 *       - m_reflectDepthRBO (DEPTH_COMPONENT24 renderbuffer)
 *
 * Notes:
 *   - Half-resolution reflection is a performance optimization and is generally sufficient for water reflections.
 *   - Texture wrapping is clamped to edge to avoid sampling outside the valid reflection image.
 */
void Water::createOrResizeReflectionFBO(int fbW, int fbH)
{
    // Allocate reflection at half resolution for performance.
    m_fboW = std::max(1, fbW / 2);
    m_fboH = std::max(1, fbH / 2);

    if (!m_reflectFBO) glGenFramebuffers(1, &m_reflectFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_reflectFBO);

    // Color texture attachment (RGBA8).
    if (!m_reflectColorTex) glGenTextures(1, &m_reflectColorTex);
    glBindTexture(GL_TEXTURE_2D, m_reflectColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_fboW, m_fboH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_reflectColorTex, 0);

    // Depth renderbuffer attachment.
    if (!m_reflectDepthRBO) glGenRenderbuffers(1, &m_reflectDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_reflectDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_fboW, m_fboH);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_reflectDepthRBO);

    // Single render target.
    GLenum drawBufs[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBufs);

    // Validate FBO completeness.
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[Water] Reflection FBO incomplete. status=" << status << "\n";
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/*
 * Initializes water resources and reflection framebuffer.
 *
 * Parameters:
 *   assetsRoot : Root directory used to locate water shader files.
 *   fbW, fbH   : Main framebuffer dimensions (used to size the reflection buffer).
 *   waterY     : Base water plane height in world units (before wave displacement).
 *   minX,maxX  : Water plane X extent in world units.
 *   minZ,maxZ  : Water plane Z extent in world units.
 *
 * Returns:
 *   true on success; false if shader loading fails.
 *
 * Notes:
 *   - Mesh resolution is fixed here (220x220 segments). Excessive segment counts can be expensive due to vertex processing.
 */
bool Water::init(const std::filesystem::path& assetsRoot,
                 int fbW, int fbH,
                 float waterY,
                 float minX, float maxX,
                 float minZ, float maxZ)
{
    shutdown();

    m_waterY = waterY;

    // Load water shaders.
    if (!m_shader.loadFromFiles((assetsRoot / "shaders/water.vert").string(),
                                (assetsRoot / "shaders/water.frag").string())) {
        std::cerr << "[Water] water shader load failed\n";
        return false;
    }

    // Create plane mesh (avoid extreme segment counts to limit triangle count).
    m_mesh = createPlaneMesh(minX, maxX, minZ, maxZ, 220, 220);

    createOrResizeReflectionFBO(fbW, fbH);
    return true;
}

/*
 * Releases reflection framebuffer resources.
 *
 * Notes:
 *   - Mesh and shader cleanup (if any) are handled by their respective owners/types.
 *   - Safe to call multiple times.
 */
void Water::shutdown()
{
    if (m_reflectDepthRBO) glDeleteRenderbuffers(1, &m_reflectDepthRBO);
    if (m_reflectColorTex) glDeleteTextures(1, &m_reflectColorTex);
    if (m_reflectFBO) glDeleteFramebuffers(1, &m_reflectFBO);

    m_reflectDepthRBO = 0;
    m_reflectColorTex = 0;
    m_reflectFBO = 0;
}

/*
 * Resizes the reflection framebuffer to match the main framebuffer (at half resolution).
 *
 * Parameters:
 *   fbW, fbH : Main framebuffer dimensions.
 */
void Water::resize(int fbW, int fbH)
{
    createOrResizeReflectionFBO(fbW, fbH);
}

/*
 * Begins the reflection render pass.
 *
 * Behavior:
 *   - Binds the reflection FBO.
 *   - Sets viewport to the reflection resolution.
 *   - Clears color and depth buffers.
 *
 * Notes:
 *   - Rendering code should draw only the objects that should appear reflected (typically everything except the water surface itself),
 *     using a reflection camera and an appropriate clip plane to avoid rendering geometry below the water plane.
 */
void Water::beginReflectionPass()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_reflectFBO);
    glViewport(0, 0, m_fboW, m_fboH);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

/*
 * Ends the reflection render pass and restores the main framebuffer viewport.
 *
 * Parameters:
 *   mainFbW, mainFbH : Main framebuffer dimensions to restore.
 */
void Water::endReflectionPass(int mainFbW, int mainFbH)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, mainFbW, mainFbH);
}

/*
 * Renders the animated water surface.
 *
 * Parameters:
 *   camera   : Active camera (used by LightingSystem to set camera-dependent uniforms).
 *   view     : Main camera view matrix.
 *   proj     : Main camera projection matrix.
 *   viewRef  : Reflection camera view matrix (mirrored camera used to produce uReflectTex).
 *   env      : Environment state for sun/ambient parameters.
 *   lighting : LightingSystem used to apply environment lighting uniforms to the water shader.
 *   timeSec  : Absolute time in seconds used for wave animation.
 *
 * Behavior:
 *   - Applies the same sun/ambient lighting parameters as the rest of the scene.
 *   - Binds the reflection texture and draws the water mesh.
 *
 * Notes:
 *   - Reflection strength and distortion are set here as fixed constants; they are intended as tuning parameters.
 */
void Water::render(const Camera& camera,
                   const glm::mat4& view,
                   const glm::mat4& proj,
                   const glm::mat4& viewRef,
                   const Environment& env,
                   const LightingSystem& lighting,
                   float timeSec)
{
    // Apply sun/ambient parameters consistent with the main lighting model.
    lighting.applyFromEnvironment(m_shader, camera, env);

    m_shader.use();
    m_shader.setMat4("uModel", glm::mat4(1.0f));
    m_shader.setMat4("uView", view);
    m_shader.setMat4("uProj", proj);
    m_shader.setMat4("uViewRef", viewRef);

    // Wave animation controls.
    m_shader.setFloat("uTime", timeSec);
    m_shader.setFloat("uWaterY", m_waterY);

    // Water appearance tuning parameters (linear RGB / scalar multipliers).
    m_shader.setVec3("uWaterColor", glm::vec3(0.02f, 0.15f, 0.22f));
    m_shader.setFloat("uReflectStrength", 1.0f);
    m_shader.setFloat("uDistortStrength", 0.02f);

    // Reflection render target.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_reflectColorTex);
    m_shader.setInt("uReflectTex", 0);

    m_mesh.draw();
}
