#pragma once
#include <vector>
#include <glm/glm.hpp>

#include "render/Mesh.h"

// 高度场地形：负责
// 1) 生成可渲染网格（目前用 pos+color）
// 2) 提供世界坐标高度查询 GetHeight(x,z)
// 3) 预埋 GetNormal(x,z) 便于未来坡度/水流/材质混合
class Terrain {
public:
    Terrain(int widthVerts, int depthVerts, float gridSpacing);

    // 生成地形（可反复调用重建）
    void Build(float noiseScale, float heightScale, int seed = 1337);

    Mesh& mesh() { return m_mesh; }
    const Mesh& mesh() const { return m_mesh; }

    // 世界坐标采样
    float GetHeight(float worldX, float worldZ) const;
    glm::vec3 GetNormal(float worldX, float worldZ) const;

    // 边界（用于限制玩家活动范围）
    float MinX() const { return m_origin.x; }
    float MinZ() const { return m_origin.y; }
    float MaxX() const { return m_origin.x + (m_widthVerts - 1) * m_gridSpacing; }
    float MaxZ() const { return m_origin.y + (m_depthVerts - 1) * m_gridSpacing; }

    // 预埋：未来水面会用到
    float waterHeight = 0.0f;

private:
    int m_widthVerts = 0;
    int m_depthVerts = 0;
    float m_gridSpacing = 1.0f;

    // 地形左下角世界坐标 (x,z)
    glm::vec2 m_origin = glm::vec2(0.0f);

    // 高度缓存（按格点存：ix + iz*width）
    std::vector<float> m_heights;

    Mesh m_mesh;

private:
    float SampleHeightGrid(int ix, int iz) const;
    float FBM(float x, float z, int seed) const;

    static float Clamp(float v, float lo, float hi) {
        return (v < lo) ? lo : (v > hi ? hi : v);
    }
};
