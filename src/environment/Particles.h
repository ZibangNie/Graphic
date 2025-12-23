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
    ~ParticleSystem();

    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

    // maxParticles: total budget for flame+embers+glow combined
    bool init(const std::string& vsPath,
              const std::string& fsPath,
              int maxParticles = 2000);

    void shutdown();

    void setCampfirePosition(const glm::vec3& worldPos) { m_emitterPos = worldPos; }

    // dt: seconds; now: absolute time (e.g., glfwGetTime)
    void update(float dt, float now);

    // clipPlane: same convention you use in terrain/basic shaders.
    void render(const Camera& cam,
                const glm::mat4& view,
                const glm::mat4& proj,
                const glm::vec4& clipPlane);

    // Tuning (adjust in main.cpp if you want)
    float flameEmitRate = 140.0f;  // particles / sec
    float emberEmitRate = 22.0f;   // particles / sec
    float glowEmitRate  = 7.0f;    // particles / sec (soft ground glow)

    float baseRadius = 0.18f;      // emission disk radius (world units)
    float baseHeight = 0.06f;      // small vertical spread

    float intensity = 1.15f;       // overall brightness

private:
    enum class Kind : int { Flame = 0, Ember = 1, Glow = 2 };

    struct Particle {
        glm::vec3 pos{0.f};
        glm::vec3 vel{0.f};

        float life = 0.f;
        float life0 = 1.f;
        float size0 = 0.1f;

        float seed = 0.f;
        Kind kind = Kind::Flame;
    };

    struct ParticleGPU {
        glm::vec4 posSize;       // xyz = world pos, w = size
        glm::vec4 ageSeedKind;   // x = age01, y = seed, z = kind, w = unused
    };

private:
    float randf(float a, float b);
    glm::vec3 randInDisk(float radius);
    void spawn(Kind kind);

private:
    Shader m_shader;

    GLuint m_vao = 0;
    GLuint m_vboQuad = 0;
    GLuint m_vboInst = 0;

    int m_maxParticles = 0;

    glm::vec3 m_emitterPos{0.f};

    std::vector<Particle> m_particles;
    std::vector<ParticleGPU> m_gpu;

    std::mt19937 m_rng{1337u};
    std::uniform_real_distribution<float> m_u01{0.f, 1.f};

    float m_flameAcc = 0.f;
    float m_emberAcc = 0.f;
    float m_glowAcc  = 0.f;

    float m_timeNow = 0.f;
};
