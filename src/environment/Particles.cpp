#include "environment/Particles.h"

#include <cmath>
#include <algorithm>

#include "scene/Camera.h"

static inline float saturate(float x) { return (x < 0.f) ? 0.f : (x > 1.f ? 1.f : x); }

ParticleSystem::~ParticleSystem() {
    shutdown();
}

bool ParticleSystem::init(const std::string& vsPath,
                          const std::string& fsPath,
                          int maxParticles) {
    shutdown();

    m_maxParticles = std::max(64, maxParticles);
    m_particles.reserve(m_maxParticles);
    m_gpu.reserve(m_maxParticles);

    if (!m_shader.loadFromFiles(vsPath, fsPath)) {
        return false;
    }

    // Quad (two triangles), local billboard space [-0.5, 0.5]
    // aPos.xy, aUV.xy
    const float quad[] = {
        -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f, -0.5f,  1.0f, 0.0f,
         0.5f,  0.5f,  1.0f, 1.0f,

        -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.0f, 1.0f,
    };

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_vboQuad);
    glBindBuffer(GL_ARRAY_BUFFER, m_vboQuad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    // aPos (location=0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    // aUV (location=1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    // Instance buffer
    glGenBuffers(1, &m_vboInst);
    glBindBuffer(GL_ARRAY_BUFFER, m_vboInst);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(ParticleGPU) * m_maxParticles), nullptr, GL_STREAM_DRAW);

    // iPosSize (location=2) vec4
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleGPU), (void*)offsetof(ParticleGPU, posSize));
    glVertexAttribDivisor(2, 1);

    // iAgeSeedKind (location=3) vec4
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleGPU), (void*)offsetof(ParticleGPU, ageSeedKind));
    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    std::random_device rd;
    m_rng.seed(rd());

    return true;
}

void ParticleSystem::shutdown() {
    if (m_vboInst) { glDeleteBuffers(1, &m_vboInst); m_vboInst = 0; }
    if (m_vboQuad) { glDeleteBuffers(1, &m_vboQuad); m_vboQuad = 0; }
    if (m_vao)     { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }

    m_particles.clear();
    m_gpu.clear();
    m_maxParticles = 0;
    m_flameAcc = 0.f;
    m_emberAcc = 0.f;
    m_glowAcc  = 0.f;
    m_timeNow = 0.f;
}

float ParticleSystem::randf(float a, float b) {
    return a + (b - a) * m_u01(m_rng);
}

glm::vec3 ParticleSystem::randInDisk(float radius) {
    float a = randf(0.f, 6.2831853f);
    float r = std::sqrt(randf(0.f, 1.f)) * radius;
    return glm::vec3(std::cos(a) * r, 0.f, std::sin(a) * r);
}

void ParticleSystem::spawn(Kind kind) {
    if ((int)m_particles.size() >= m_maxParticles) {
        // Keep it lively: replace the oldest (simple + sufficient for this project).
        m_particles.erase(m_particles.begin());
    }

    Particle p;
    p.kind = kind;
    p.seed = randf(0.f, 1.f);

    if (kind == Kind::Flame) {
        glm::vec3 disk = randInDisk(baseRadius);
        p.pos = m_emitterPos + disk + glm::vec3(0.f, randf(0.f, baseHeight), 0.f);

        glm::vec3 lateral(randf(-0.55f, 0.55f), 0.f, randf(-0.55f, 0.55f));
        p.vel = lateral * 0.45f + glm::vec3(0.f, randf(1.35f, 2.35f), 0.f);

        p.life0 = randf(0.55f, 1.05f);
        p.life  = p.life0;

        p.size0 = randf(0.10f, 0.20f);
    } else if (kind == Kind::Ember) {
        glm::vec3 disk = randInDisk(baseRadius * 0.55f);
        p.pos = m_emitterPos + disk + glm::vec3(0.f, randf(0.f, baseHeight * 0.6f), 0.f);

        glm::vec3 lateral(randf(-1.0f, 1.0f), 0.f, randf(-1.0f, 1.0f));
        p.vel = lateral * 0.55f + glm::vec3(0.f, randf(1.6f, 3.2f), 0.f);

        p.life0 = randf(1.0f, 2.2f);
        p.life  = p.life0;

        p.size0 = randf(0.025f, 0.055f);
    } else {
        // Glow: fake bounce light without touching terrain shader
        glm::vec3 disk = randInDisk(baseRadius * 0.75f);
        p.pos = m_emitterPos + disk + glm::vec3(0.f, 0.03f, 0.f);
        p.vel = glm::vec3(0.f);

        p.life0 = randf(0.18f, 0.30f);
        p.life  = p.life0;

        p.size0 = randf(0.45f, 0.70f);
    }

    m_particles.push_back(p);
}

void ParticleSystem::update(float dt, float now) {
    if (dt <= 0.f) return;

    dt = std::min(dt, 0.05f);
    m_timeNow = now;

    // Spawn accumulators
    m_flameAcc += dt * flameEmitRate;
    m_emberAcc += dt * emberEmitRate;
    m_glowAcc  += dt * glowEmitRate;

    int nFlame = (int)std::floor(m_flameAcc);
    int nEmber = (int)std::floor(m_emberAcc);
    int nGlow  = (int)std::floor(m_glowAcc);

    m_flameAcc -= (float)nFlame;
    m_emberAcc -= (float)nEmber;
    m_glowAcc  -= (float)nGlow;

    for (int i = 0; i < nFlame; ++i) spawn(Kind::Flame);
    for (int i = 0; i < nEmber; ++i) spawn(Kind::Ember);
    for (int i = 0; i < nGlow;  ++i) spawn(Kind::Glow);

    // Integrate
    for (int i = (int)m_particles.size() - 1; i >= 0; --i) {
        Particle& p = m_particles[i];

        p.life -= dt;
        if (p.life <= 0.f) {
            m_particles[i] = m_particles.back();
            m_particles.pop_back();
            continue;
        }

        float age01 = 1.0f - (p.life / p.life0);

        float t = now * 1.2f + p.seed * 17.0f;
        glm::vec3 turb(std::sin(t * 3.1f), 0.f, std::cos(t * 2.7f));

        if (p.kind == Kind::Flame) {
            p.vel += glm::vec3(0.f, 2.4f, 0.f) * dt;
            p.vel += turb * (1.35f * (1.0f - age01)) * dt;
            p.vel *= std::exp(-1.8f * dt);
        } else if (p.kind == Kind::Ember) {
            p.vel += glm::vec3(0.f, 0.9f, 0.f) * dt;
            p.vel += turb * 0.35f * dt;
            p.vel += glm::vec3(0.f, -2.2f, 0.f) * dt;
            p.vel *= std::exp(-0.9f * dt);
        } else {
            p.vel = glm::vec3(0.f);
        }

        p.pos += p.vel * dt;
    }

    // Build GPU list
    m_gpu.clear();
    m_gpu.reserve(m_particles.size());

    for (const Particle& p : m_particles) {
        float age01 = 1.0f - (p.life / p.life0);

        float size = p.size0;
        if (p.kind == Kind::Flame) {
            size *= (0.85f + 0.85f * age01);
        } else if (p.kind == Kind::Ember) {
            size *= (1.0f - 0.35f * age01);
        } else {
            size *= (0.95f + 0.10f * std::sin(m_timeNow * 8.0f + p.seed * 30.0f));
        }

        ParticleGPU g;
        g.posSize = glm::vec4(p.pos, size);
        g.ageSeedKind = glm::vec4(saturate(age01), p.seed, (float)((int)p.kind), 0.f);
        m_gpu.push_back(g);
    }

    // Upload instance buffer
    glBindBuffer(GL_ARRAY_BUFFER, m_vboInst);
    if (!m_gpu.empty()) {
        glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(m_gpu.size() * sizeof(ParticleGPU)), m_gpu.data());
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ParticleSystem::render(const Camera& cam,
                            const glm::mat4& view,
                            const glm::mat4& proj,
                            const glm::vec4& clipPlane) {
    if (!m_vao || m_gpu.empty()) return;

    glm::vec3 camFwd = cam.forward();
    glm::vec3 camRight = cam.right();
    glm::vec3 camUp = glm::normalize(glm::cross(camRight, camFwd));

    m_shader.use();
    m_shader.setMat4("uView", view);
    m_shader.setMat4("uProj", proj);
    m_shader.setVec3("uCamRight", camRight);
    m_shader.setVec3("uCamUp", camUp);
    m_shader.setVec4("uClipPlane", clipPlane);
    m_shader.setFloat("uTime", m_timeNow);
    m_shader.setFloat("uIntensity", intensity);

    GLboolean blendWas = glIsEnabled(GL_BLEND);
    GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
    GLboolean cullWas  = glIsEnabled(GL_CULL_FACE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glDisable(GL_CULL_FACE);

    glBindVertexArray(m_vao);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)m_gpu.size());
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);

    if (!blendWas) glDisable(GL_BLEND);
    if (!depthWas) glDisable(GL_DEPTH_TEST);
    if (cullWas) glEnable(GL_CULL_FACE);
}
