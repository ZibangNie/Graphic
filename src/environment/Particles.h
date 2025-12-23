/*
 * Particles.h
 *
 * Purpose:
 *   Declares ParticleSystem, a compact GPU-instanced billboard particle system intended for a small campfire.
 *   Simulates particles on the CPU and streams per-instance data to the GPU each frame.
 *
 * Rendering model:
 *   - One static quad mesh (two triangles) + instancing for all particles.
 *   - Additive blending for fire/glow accumulation.
 *   - Procedural shading in the particle fragment shader (no particle texture required).
 *
 * Typical integration:
 *   - init() once during startup with shader paths and a particle budget.
 *   - setCampfirePosition() whenever the emitter should move (e.g., follow a character).
 *   - update(dt, now) once per frame to simulate and upload instance data.
 *   - render(...) once per frame to draw with the current camera/view/projection and clip plane.
 *
 * Notes:
 *   - Includes glad here because the header exposes OpenGL types (GLuint) and is responsible for resource handles.
 */

#pragma once

#include <vector>
#include <string>
#include <random>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "render/Shader.h"

class Camera;

// Simple, efficient billboard particle system for a small campfire (flame + embers + subtle ground glow).
// - GPU instancing (one quad, many instances)
// - Additive blending
// - Procedural flame shading (no texture required)
class ParticleSystem {
public:
    ParticleSystem() = default;

    /*
     * Destructor.
     *
     * Side effects:
     *   - Releases GPU resources via shutdown().
     */
    ~ParticleSystem();

    // Non-copyable: owns OpenGL resources (VAO/VBOs) and CPU-side particle storage.
    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

    /*
     * Initializes GL resources and loads the particle shaders.
     *
     * Parameters:
     *   vsPath        : Vertex shader file path.
     *   fsPath        : Fragment shader file path.
     *   maxParticles  : Maximum number of live particles (shared budget across flame/embers/glow).
     *                  Recommended range: [256, 5000] depending on performance budget.
     *
     * Returns:
     *   true on success; false if shader loading fails.
     */
    bool init(const std::string& vsPath,
              const std::string& fsPath,
              int maxParticles = 2000);

    /*
     * Releases GL resources and clears simulation state.
     */
    void shutdown();

    /*
     * Sets the world-space emitter origin for newly spawned particles.
     *
     * Parameters:
     *   worldPos : Emitter position in world space (center of the campfire).
     */
    void setCampfirePosition(const glm::vec3& worldPos) { m_emitterPos = worldPos; }

    /*
     * Advances particle simulation and uploads per-instance data.
     *
     * Parameters:
     *   dt  : Delta time in seconds. Expected to be non-negative; may be clamped internally for stability.
     *   now : Absolute time in seconds (e.g., glfwGetTime), used for procedural turbulence/flicker phase.
     */
    void update(float dt, float now);

    /*
     * Renders the particle system using instanced billboards.
     *
     * Parameters:
     *   cam       : Camera used to derive billboard basis vectors (right/up).
     *   view      : View matrix for the current render pass.
     *   proj      : Projection matrix for the current render pass.
     *   clipPlane : World-space clip plane (A,B,C,D). Same sign convention as terrain/basic shaders.
     *
     * Notes:
     *   - Rendering typically uses additive blending and disables depth writes (implementation detail in .cpp).
     */
    void render(const Camera& cam,
                const glm::mat4& view,
                const glm::mat4& proj,
                const glm::vec4& clipPlane);

    // ---- Runtime tuning knobs (commonly adjusted in main.cpp) ----

    // Emission rates (particles per second). Higher values increase density and GPU fill cost.
    float flameEmitRate = 140.0f;  // Particles / sec (flame body).
    float emberEmitRate = 22.0f;   // Particles / sec (sparks).
    float glowEmitRate  = 7.0f;    // Particles / sec (soft ground glow).

    // Emitter shape (world units).
    float baseRadius = 0.18f;      // Radius of the emission disk on XZ plane.
    float baseHeight = 0.06f;      // Small initial vertical spread above the emitter.

    // Global brightness multiplier forwarded to the shader.
    float intensity = 1.15f;       // Typical range ~[0.5, 2.0].

private:
    // Particle kinds; encoded into GPU instance data for shader branching.
    enum class Kind : int { Flame = 0, Ember = 1, Glow = 2 };

    // CPU particle state.
    struct Particle {
        glm::vec3 pos{0.f};
        glm::vec3 vel{0.f};

        float life = 0.f;   // Remaining lifetime in seconds.
        float life0 = 1.f;  // Initial lifetime in seconds (used to compute normalized age).
        float size0 = 0.1f; // Base billboard size in world units.

        float seed = 0.f;   // Per-particle random seed in [0,1].
        Kind kind = Kind::Flame;
    };

    // GPU instance payload streamed each frame (matches particle.vert attribute layout).
    struct ParticleGPU {
        glm::vec4 posSize;       // xyz = world position, w = size (world units).
        glm::vec4 ageSeedKind;   // x = age01 [0,1], y = seed [0,1], z = kind (0/1/2), w = unused.
    };

private:
    /*
     * Utility: returns a uniform random float in [a, b].
     */
    float randf(float a, float b);

    /*
     * Utility: returns a random point within a disk on the XZ plane (y = 0).
     */
    glm::vec3 randInDisk(float radius);

    /*
     * Spawns a single particle of the given kind using kind-specific parameter ranges.
     */
    void spawn(Kind kind);

private:
    Shader m_shader;

    // OpenGL resources.
    GLuint m_vao = 0;
    GLuint m_vboQuad = 0;  // Static quad geometry.
    GLuint m_vboInst = 0;  // Streamed instance buffer.

    int m_maxParticles = 0;

    // Emitter origin in world space.
    glm::vec3 m_emitterPos{0.f};

    // CPU particle list and GPU upload list.
    std::vector<Particle> m_particles;
    std::vector<ParticleGPU> m_gpu;

    // RNG for particle parameter variation.
    std::mt19937 m_rng{1337u};
    std::uniform_real_distribution<float> m_u01{0.f, 1.f};

    // Emission accumulators (fractional spawn tracking).
    float m_flameAcc = 0.f;
    float m_emberAcc = 0.f;
    float m_glowAcc  = 0.f;

    // Cached absolute time for shader uniforms and size pulsation.
    float m_timeNow = 0.f;
};
