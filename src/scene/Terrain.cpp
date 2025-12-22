#include "scene/Terrain.h"
#include <cmath>
#include <cstdint>
#include <algorithm>

static inline float Smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

static inline float Hash2D(int x, int z, int seed) {
    // 稳定可复现的整数哈希 -> [0,1)
    uint32_t h = (uint32_t)(x * 374761393 + z * 668265263) ^ (uint32_t)seed;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= (h >> 16);
    return (h & 0x00FFFFFF) / float(0x01000000);
}

static float ValueNoise(float x, float z, int seed) {
    int x0 = (int)std::floor(x);
    int z0 = (int)std::floor(z);
    int x1 = x0 + 1;
    int z1 = z0 + 1;

    float tx = x - (float)x0;
    float tz = z - (float)z0;

    float a = Hash2D(x0, z0, seed);
    float b = Hash2D(x1, z0, seed);
    float c = Hash2D(x0, z1, seed);
    float d = Hash2D(x1, z1, seed);

    float ux = Smoothstep(tx);
    float uz = Smoothstep(tz);

    float ab = a + (b - a) * ux;
    float cd = c + (d - c) * ux;
    return ab + (cd - ab) * uz; // [0,1)
}

Terrain::Terrain(int widthVerts, int depthVerts, float gridSpacing)
    : m_widthVerts(widthVerts), m_depthVerts(depthVerts), m_gridSpacing(gridSpacing) {

    // 将地形中心放在 (0,0) 附近，方便你现在玩家初始就在原点附近
    float width = (m_widthVerts - 1) * m_gridSpacing;
    float depth = (m_depthVerts - 1) * m_gridSpacing;
    m_origin = glm::vec2(-width * 0.5f, -depth * 0.5f);

    m_heights.resize((size_t)m_widthVerts * (size_t)m_depthVerts, 0.0f);
}

float Terrain::FBM(float x, float z, int seed) const {
    float sum = 0.0f;
    float amp = 0.5f;
    float freq = 1.0f;
    for (int i = 0; i < 5; ++i) {
        float n = ValueNoise(x * freq, z * freq, seed + i * 17); // [0,1)
        n = n * 2.0f - 1.0f; // [-1,1]
        sum += amp * n;
        freq *= 2.0f;
        amp *= 0.5f;
    }
    return sum; // ~[-1,1]
}

float Terrain::SampleHeightGrid(int ix, int iz) const {
    ix = (int)Clamp((float)ix, 0.0f, (float)m_widthVerts - 1.0f);
    iz = (int)Clamp((float)iz, 0.0f, (float)m_depthVerts - 1.0f);
    return m_heights[(size_t)ix + (size_t)iz * (size_t)m_widthVerts];
}

void Terrain::Build(float noiseScale, float heightScale, int seed) {
    // 1) Generate heightfield
    for (int z = 0; z < m_depthVerts; ++z) {
        for (int x = 0; x < m_widthVerts; ++x) {
            float wx = m_origin.x + x * m_gridSpacing;
            float wz = m_origin.y + z * m_gridSpacing;

            float h = FBM(wx * noiseScale, wz * noiseScale, seed) * heightScale;
            m_heights[(size_t)x + (size_t)z * (size_t)m_widthVerts] = h;
        }
    }

    // 2) Build triangle mesh (pos + color), 2 triangles per cell
    std::vector<float> v;
    v.reserve((size_t)(m_widthVerts - 1) * (size_t)(m_depthVerts - 1) * 6ull * 6ull);

    auto push = [&](float px, float py, float pz, const glm::vec3& c) {
        v.push_back(px); v.push_back(py); v.push_back(pz);
        v.push_back(c.r); v.push_back(c.g); v.push_back(c.b);
    };

    // Height-based color ramp (easy to tune)
    auto colorFromHeight = [&](float h) -> glm::vec3 {
        // ---- Tunable thresholds (world height units) ----
        const float water = waterHeight;     // e.g. -0.5
        const float beach = water + 0.25f;   // sand band thickness above water
        const float grass = water + 1.20f;   // mostly grass up to here
        const float rock  = water + 2.60f;   // grass -> rock transition ends here
        const float snow  = water + 3.40f;   // rock -> snow transition ends here

        // ---- Tunable colors ----
        const glm::vec3 cUnderDeep = {0.05f, 0.12f, 0.20f};
        const glm::vec3 cUnderShal = {0.08f, 0.20f, 0.30f};
        const glm::vec3 cBeach     = {0.76f, 0.70f, 0.46f};
        const glm::vec3 cGrass     = {0.18f, 0.55f, 0.20f};
        const glm::vec3 cRock      = {0.45f, 0.42f, 0.40f};
        const glm::vec3 cSnow      = {0.88f, 0.88f, 0.92f};

        auto clamp01 = [](float x) { return std::max(0.0f, std::min(1.0f, x)); };
        auto lerp = [](const glm::vec3& a, const glm::vec3& b, float t) { return a + (b - a) * t; };

        // Underwater
        if (h <= water) {
            float t = clamp01((h - (water - 2.0f)) / 2.0f); // water-2 -> water
            return lerp(cUnderDeep, cUnderShal, t);
        }

        // Beach
        if (h <= beach) {
            float t = clamp01((h - water) / (beach - water));
            return lerp(cUnderShal, cBeach, t);
        }

        // Grass plateau (beach -> grass)
        if (h <= grass) {
            float t = clamp01((h - beach) / std::max(1e-6f, (grass - beach)));
            return lerp(cBeach, cGrass, t * t); // bias
        }

        // Grass -> Rock (grass -> rock)
        if (h <= rock) {
            float t = clamp01((h - grass) / std::max(1e-6f, (rock - grass)));
            return lerp(cGrass, cRock, t * t);
        }

        // Rock -> Snow (rock -> snow)
        {
            float t = clamp01((h - rock) / std::max(1e-6f, (snow - rock)));
            return lerp(cRock, cSnow, t);
        }
    };

    // Build geometry
    for (int z = 0; z < m_depthVerts - 1; ++z) {
        for (int x = 0; x < m_widthVerts - 1; ++x) {
            float x0 = m_origin.x + x * m_gridSpacing;
            float x1 = m_origin.x + (x + 1) * m_gridSpacing;
            float z0 = m_origin.y + z * m_gridSpacing;
            float z1 = m_origin.y + (z + 1) * m_gridSpacing;

            float h00 = SampleHeightGrid(x,     z);
            float h10 = SampleHeightGrid(x + 1, z);
            float h01 = SampleHeightGrid(x,     z + 1);
            float h11 = SampleHeightGrid(x + 1, z + 1);

            glm::vec3 c00 = colorFromHeight(h00);
            glm::vec3 c10 = colorFromHeight(h10);
            glm::vec3 c01 = colorFromHeight(h01);
            glm::vec3 c11 = colorFromHeight(h11);

            // tri 1: (x0,z0) (x1,z0) (x1,z1)
            push(x0, h00, z0, c00);
            push(x1, h10, z0, c10);
            push(x1, h11, z1, c11);

            // tri 2: (x0,z0) (x1,z1) (x0,z1)
            push(x0, h00, z0, c00);
            push(x1, h11, z1, c11);
            push(x0, h01, z1, c01);
        }
    }

    // Upload to GPU
    m_mesh.uploadInterleavedPosColor(v);
}

float Terrain::GetHeight(float worldX, float worldZ) const {
    // world -> local grid coords
    float lx = (worldX - m_origin.x) / m_gridSpacing;
    float lz = (worldZ - m_origin.y) / m_gridSpacing;

    int x0 = (int)std::floor(lx);
    int z0 = (int)std::floor(lz);

    // clamp to valid cell range
    x0 = (int)Clamp((float)x0, 0.0f, (float)m_widthVerts - 2.0f);
    z0 = (int)Clamp((float)z0, 0.0f, (float)m_depthVerts - 2.0f);

    float tx = lx - (float)x0;
    float tz = lz - (float)z0;
    tx = Clamp(tx, 0.0f, 1.0f);
    tz = Clamp(tz, 0.0f, 1.0f);

    float h00 = SampleHeightGrid(x0,     z0);
    float h10 = SampleHeightGrid(x0 + 1, z0);
    float h01 = SampleHeightGrid(x0,     z0 + 1);
    float h11 = SampleHeightGrid(x0 + 1, z0 + 1);

    float hx0 = h00 + (h10 - h00) * tx;
    float hx1 = h01 + (h11 - h01) * tx;
    return hx0 + (hx1 - hx0) * tz;
}

glm::vec3 Terrain::GetNormal(float worldX, float worldZ) const {
    // 用中心差分近似法线（后续做坡度/水流/材质混合会很有用）
    float eps = m_gridSpacing;
    float hL = GetHeight(worldX - eps, worldZ);
    float hR = GetHeight(worldX + eps, worldZ);
    float hD = GetHeight(worldX, worldZ - eps);
    float hU = GetHeight(worldX, worldZ + eps);

    glm::vec3 n(-(hR - hL), 2.0f * eps, -(hU - hD));
    float len = std::sqrt(n.x*n.x + n.y*n.y + n.z*n.z);
    if (len < 1e-6f) return glm::vec3(0,1,0);
    return n / len;
}
