/*
 * Terrain.h
 *
 * Purpose:
 *   Heightfield terrain generator and sampler.
 *   Responsibilities:
 *     1) Generate a renderable mesh for the terrain (currently using an interleaved pos + "color" format).
 *     2) Provide world-space height queries via GetHeight(x,z) for gameplay and placement.
 *     3) Provide world-space normal queries via GetNormal(x,z) for lighting and future slope/material logic.
 *
 * Data layout:
 *   - The terrain is defined on an XZ grid with Y as height.
 *   - Heights are cached per grid vertex in a flattened array: index = ix + iz * widthVerts.
 *   - m_origin stores the world-space position of the grid's minimum corner (x,z).
 *
 * Rendering convention:
 *   - Mesh generation uses Mesh::uploadInterleavedPosColor().
 *   - The "color" attribute may be repurposed (e.g., to store normals) depending on shader usage.
 */

#pragma once
#include <vector>
#include <glm/glm.hpp>

#include "render/Mesh.h"

// Heightfield terrain:
//   1) Generates a renderable mesh (currently pos + color attribute stream).
//   2) Provides world-space height sampling via GetHeight(x,z).
//   3) Provides world-space normals via GetNormal(x,z) for lighting and future extensions.
class Terrain {
public:
    /*
     * Constructs a terrain grid definition.
     *
     * Parameters:
     *   widthVerts   : Number of vertices along X (>= 2 recommended).
     *   depthVerts   : Number of vertices along Z (>= 2 recommended).
     *   gridSpacing  : World-space spacing between adjacent vertices (world units).
     *
     * Notes:
     *   - The grid is centered around the world origin on XZ by choosing m_origin accordingly.
     *   - Heights are initialized to 0 until Build() is called.
     */
    Terrain(int widthVerts, int depthVerts, float gridSpacing);

    /*
     * Builds (or rebuilds) the heightfield and updates the GPU mesh.
     *
     * Parameters:
     *   noiseScale  : Scales world XZ coordinates before noise evaluation.
     *                Smaller values -> larger terrain features; typical range ~0.03 to 0.20.
     *   heightScale : Vertical amplitude in world units; typical range depends on scene scale.
     *   seed        : Deterministic seed for reproducible terrain generation.
     */
    void Build(float noiseScale, float heightScale, int seed = 1337);

    // Access to the generated render mesh.
    Mesh& mesh() { return m_mesh; }
    const Mesh& mesh() const { return m_mesh; }

    // World-space sampling.
    // GetHeight performs bilinear interpolation over the cached grid heights.
    float GetHeight(float worldX, float worldZ) const;

    // Returns an approximate world-space normal (unit vector), typically via finite differences.
    glm::vec3 GetNormal(float worldX, float worldZ) const;

    // World-space bounds of the generated grid (useful for clamping movement).
    float MinX() const { return m_origin.x; }
    float MinZ() const { return m_origin.y; }
    float MaxX() const { return m_origin.x + (m_widthVerts - 1) * m_gridSpacing; }
    float MaxZ() const { return m_origin.y + (m_depthVerts - 1) * m_gridSpacing; }

    // Water reference height in world units (used as a configurable threshold by terrain logic/shaders).
    float waterHeight = 0.0f;

private:
    int m_widthVerts = 0;
    int m_depthVerts = 0;
    float m_gridSpacing = 1.0f;

    // World-space origin of the grid at (ix=0, iz=0), stored as (x,z).
    glm::vec2 m_origin = glm::vec2(0.0f);

    // Cached heights per grid vertex: heights[ix + iz * widthVerts].
    std::vector<float> m_heights;

    // Renderable mesh generated from the heightfield.
    Mesh m_mesh;

private:
    /*
     * Returns the cached height at integer grid coordinates with clamping.
     *
     * Parameters:
     *   ix, iz : Grid indices.
     *
     * Returns:
     *   Height (Y) in world units for the nearest cached vertex.
     */
    float SampleHeightGrid(int ix, int iz) const;

    /*
     * Fractal noise function used by Build() to synthesize terrain elevation.
     *
     * Parameters:
     *   x, z : Noise-space coordinates (typically world coords scaled by noiseScale).
     *   seed : Deterministic seed.
     *
     * Returns:
     *   A signed value (typically ~[-1,1], not strictly bounded).
     */
    float FBM(float x, float z, int seed) const;

    // Utility clamp used across sampling and generation.
    static float Clamp(float v, float lo, float hi) {
        return (v < lo) ? lo : (v > hi ? hi : v);
    }
};
