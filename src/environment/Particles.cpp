/*
 * Particles.cpp
 *
 * Purpose:
 *   Implements a lightweight, GPU-instanced particle system used for a small campfire effect.
 *   Particles are simulated on the CPU and rendered as camera-facing billboards using instancing.
 *
 * Rendering approach:
 *   - A unit quad (two triangles) is stored in a static VBO.
 *   - Per-particle instance data (world position, size, age, seed, kind) is streamed each frame to a VBO.
 *   - The vertex shader expands billboards using camera right/up vectors; the fragment shader is procedural.
 *
 * Blending / depth:
 *   - Uses additive blending (GL_SRC_ALPHA, GL_ONE) for fire/glow accumulation.
 *   - Depth test is enabled; depth writes are disabled during rendering to reduce artifacts.
 *   - Face culling is disabled for billboards.
 *
 * Notes:
 *   - Particle spawning uses simple rate accumulators to achieve stable emission rates across variable dt.
 *   - When at capacity, the oldest particle is replaced to keep the effect visually active.
 */

#include "environment/Particles.h"

#include <cmath>
#include <algorithm>

#include "scene/Camera.h"

// Clamps to [0,1]. Used for normalizing age values for shader consumption.
static inline float saturate(float x) { return (x < 0.f) ? 0.f : (x > 1.f ? 1.f : x); }

/*
 * Destructor.
 *
 * Side effects:
 *   - Releases OpenGL buffers/VAO and clears CPU/GPU particle storage.
 */
ParticleSystem::~ParticleSystem() {
    shutdown();
}

/*
 * Initializes the particle system GPU resources and shader program.
 *
 * Parameters:
 *   vsPath        : Vertex shader file path for the particle renderer.
 *   fsPath        : Fragment shader file path for the particle renderer.
 *   maxParticles  : Upper bound for live particles; clamped to a minimum of 64.
 *
 * Returns:
 *   true on successful shader load and GL resource creation; false if shader compilation/linking fails.
 *
 * Side effects:
 *   - Allocates VAO/VBOs, configures vertex attributes and instancing divisors.
 *   - Seeds the RNG used for particle parameter randomization.
 */
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

    // Unit quad in local billboard space: aPos.xy in [-0.5, 0.5], aUV.xy in [0,1].
    // Layout per-vertex: [pos.x, pos.y, uv.x, uv.y].
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

    // Static quad vertex buffer.
    glGenBuffers(1, &m_vboQuad);
    glBindBuffer(GL_ARRAY_BUFFER, m_vboQuad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    // aPos (location=0): vec2, stride = 4 floats.
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    // aUV (location=1): vec2, offset = 2 floats.
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    // Instance buffer: streamed each frame.
    glGenBuffers(1, &m_vboInst);
    glBindBuffer(GL_ARRAY_BUFFER, m_vboInst);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(ParticleGPU) * m_maxParticles), nullptr, GL_STREAM_DRAW);

    // iPosSize (location=2): vec4, per-instance.
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleGPU), (void*)offsetof(ParticleGPU, posSize));
    glVertexAttribDivisor(2, 1);

    // iAgeSeedKind (location=3): vec4, per-instance.
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleGPU), (void*)offsetof(ParticleGPU, ageSeedKind));
    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Seed RNG for particle variations (positions, velocities, lifetimes, sizes).
    std::random_device rd;
    m_rng.seed(rd());

    return true;
}

/*
 * Releases GPU resources and resets simulation state.
 *
 * Side effects:
 *   - Deletes VAO/VBOs if allocated.
 *   - Clears particle arrays and resets counters/accumulators.
 */
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

/*
 * Returns a uniform random float in [a, b].
 *
 * Parameters:
 *   a, b : Range endpoints (a can be <= b).
 */
float ParticleSystem::randf(float a, float b) {
    return a + (b - a) * m_u01(m_rng);
}

/*
 * Samples a random point within a disk on the XZ plane.
 *
 * Parameters:
 *   radius : Disk radius in world units.
 *
 * Returns:
 *   A vector (x, 0, z) within the disk. Uses sqrt(r) to ensure uniform area distribution.
 */
glm::vec3 ParticleSystem::randInDisk(float radius) {
    float a = randf(0.f, 6.2831853f);
    float r = std::sqrt(randf(0.f, 1.f)) * radius;
    return glm::vec3(std::cos(a) * r, 0.f, std::sin(a) * r);
}

/*
 * Spawns a single particle of the given kind using preset ranges for initial state.
 *
 * Parameters:
 *   kind : Particle kind (Flame, Ember, Glow).
 *
 * Behavior:
 *   - If capacity is exceeded, removes the oldest particle to make room.
 *   - Initializes position, velocity, lifetime, and base size based on kind.
 *
 * Notes:
 *   - Flame and ember lifetimes are longer and move upward; glow is short-lived and stationary near the ground.
 */
void ParticleSystem::spawn(Kind kind) {
    if ((int)m_particles.size() >= m_maxParticles) {
        // Replace the oldest particle to keep emission visually continuous at capacity.
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
        // Glow: short-lived additive disk near the ground to suggest bounce light.
        glm::vec3 disk = randInDisk(baseRadius * 0.75f);
        p.pos = m_emitterPos + disk + glm::vec3(0.f, 0.03f, 0.f);
        p.vel = glm::vec3(0.f);

        p.life0 = randf(0.18f, 0.30f);
        p.life  = p.life0;

        p.size0 = randf(0.45f, 0.70f);
    }

    m_particles.push_back(p);
}

/*
 * Advances simulation and streams instance data to the GPU.
 *
 * Parameters:
 *   dt  : Delta time in seconds. Clamped to a maximum step to stabilize simulation under frame spikes.
 *   now : Absolute time in seconds (used for turbulence and flicker phase).
 *
 * Behavior:
 *   - Uses emission rate accumulators to spawn a stable number of particles each frame.
 *   - Integrates particle velocity and position with simple forces and exponential damping.
 *   - Rebuilds the GPU instance list and uploads it via glBufferSubData().
 *
 * Notes:
 *   - dt is clamped to 0.05 to reduce instability when the application stalls.
 *   - Age is normalized as age01 = 1 - life/life0 and clamped to [0,1] for shader use.
 */
void ParticleSystem::update(float dt, float now) {
    if (dt <= 0.f) return;

    // Clamp timestep to reduce instability on large frame times.
    dt = std::min(dt, 0.05f);
    m_timeNow = now;

    // Spawn accumulators: accumulator increases by (rate * dt); integer part spawns count.
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

    // Integrate: iterate backward to allow swap-and-pop removal.
    for (int i = (int)m_particles.size() - 1; i >= 0; --i) {
        Particle& p = m_particles[i];

        p.life -= dt;
        if (p.life <= 0.f) {
            // Remove dead particle by swapping with the back element.
            m_particles[i] = m_particles.back();
            m_particles.pop_back();
            continue;
        }

        float age01 = 1.0f - (p.life / p.life0);

        // Low-cost pseudo-turbulence based on time and particle seed.
        float t = now * 1.2f + p.seed * 17.0f;
        glm::vec3 turb(std::sin(t * 3.1f), 0.f, std::cos(t * 2.7f));

        if (p.kind == Kind::Flame) {
            // Upward acceleration + stronger turbulence early in life, plus strong damping.
            p.vel += glm::vec3(0.f, 2.4f, 0.f) * dt;
            p.vel += turb * (1.35f * (1.0f - age01)) * dt;
            p.vel *= std::exp(-1.8f * dt);
        } else if (p.kind == Kind::Ember) {
            // Slight upward push, wind, and gravity-like pull with moderate damping.
            p.vel += glm::vec3(0.f, 0.9f, 0.f) * dt;
            p.vel += turb * 0.35f * dt;
            p.vel += glm::vec3(0.f, -2.2f, 0.f) * dt;
            p.vel *= std::exp(-0.9f * dt);
        } else {
            // Glow remains stationary (visual-only disk).
            p.vel = glm::vec3(0.f);
        }

        p.pos += p.vel * dt;
    }

    // Build GPU instance list (ParticleGPU) for instanced rendering.
    m_gpu.clear();
    m_gpu.reserve(m_particles.size());

    for (const Particle& p : m_particles) {
        float age01 = 1.0f - (p.life / p.life0);

        // Size evolution per kind to enhance readability (flames grow, embers shrink, glow pulses).
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
        // ageSeedKind.z encodes kind as a float for the shader (0 flame, 1 ember, 2 glow).
        g.ageSeedKind = glm::vec4(saturate(age01), p.seed, (float)((int)p.kind), 0.f);
        m_gpu.push_back(g);
    }

    // Upload instance buffer (streamed); only updates the used prefix.
    glBindBuffer(GL_ARRAY_BUFFER, m_vboInst);
    if (!m_gpu.empty()) {
        glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(m_gpu.size() * sizeof(ParticleGPU)), m_gpu.data());
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/*
 * Renders the particle system as instanced camera-facing billboards.
 *
 * Parameters:
 *   cam       : Camera object used to derive world-space basis vectors (forward/right/up).
 *   view      : View matrix for rendering pass.
 *   proj      : Projection matrix for rendering pass.
 *   clipPlane : World-space clip plane (A,B,C,D). Passed to the particle vertex shader via gl_ClipDistance.
 *
 * Behavior:
 *   - Configures additive blending and disables depth writes (keeps depth test enabled).
 *   - Disables face culling for billboards.
 *   - Restores previous GL enable states for blend/depth/cull (depth mask is also restored).
 *
 * Notes:
 *   - Additive blending is appropriate for fire/glow. For smoke-like particles, alpha blending is more typical.
 *   - camUp is recomputed as cross(camRight, camFwd) to ensure orthogonality.
 */
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

    // Preserve existing GL enable states for blend/depth/cull to avoid side effects.
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

    // Restore depth writes; restore GL enables to their prior state.
    glDepthMask(GL_TRUE);

    if (!blendWas) glDisable(GL_BLEND);
    if (!depthWas) glDisable(GL_DEPTH_TEST);
    if (cullWas) glEnable(GL_CULL_FACE);
}
