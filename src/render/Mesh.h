/*
 * Mesh.h
 *
 * Purpose:
 *   Declares Mesh, a small RAII wrapper around OpenGL VAO/VBO/EBO objects.
 *   Provides upload helpers for a few fixed interleaved vertex formats and a draw() method that
 *   selects indexed vs non-indexed rendering based on whether an index buffer is present.
 *
 * Supported vertex formats (interleaved):
 *   1) Pos + Color
 *      - 6 floats per vertex: [px, py, pz, cr, cg, cb]
 *      - Attribute locations: 0=pos (vec3), 1=color (vec3)
 *
 *   2) Pos + Normal + UV
 *      - 8 floats per vertex: [px, py, pz, nx, ny, nz, u, v]
 *      - Attribute locations: 0=pos (vec3), 1=normal (vec3), 2=uv (vec2)
 *
 * Ownership / lifetime:
 *   - Mesh owns its OpenGL handles and deletes them in the destructor.
 *   - Copying is disabled (resource ownership); moving is supported.
 *
 * Notes:
 *   - OpenGL context must be valid when uploading data and when the destructor runs.
 */

#pragma once
#include <vector>
#include <cstdint>
#include <glad/glad.h>

class Mesh {
public:
    Mesh() = default;

    /*
     * Destructor.
     *
     * Side effects:
     *   - Deletes VAO/VBO/EBO handles if created.
     */
    ~Mesh();

    // Non-copyable: owns OpenGL resources.
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    // Movable: transfers OpenGL resource ownership.
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    /*
     * Uploads a non-indexed mesh with interleaved position + color data.
     *
     * Parameters:
     *   vertices : Interleaved vertex data (6 floats per vertex):
     *              [px, py, pz, cr, cg, cb]
     *
     * Notes:
     *   - Sets m_indexCount to 0 (drawArrays path).
     */
    void uploadInterleavedPosColor(const std::vector<float>& vertices);

    /*
     * Uploads a non-indexed mesh with interleaved position + normal + UV data.
     *
     * Parameters:
     *   vertices : Interleaved vertex data (8 floats per vertex):
     *              [px, py, pz, nx, ny, nz, u, v]
     *
     * Notes:
     *   - Sets m_indexCount to 0 (drawArrays path).
     */
    void uploadInterleavedPosNormalUV(const std::vector<float>& vertices);

    /*
     * Uploads an indexed mesh with interleaved position + normal + UV data.
     *
     * Parameters:
     *   vertices : Interleaved vertex data (8 floats per vertex):
     *              [px, py, pz, nx, ny, nz, u, v]
     *   indices  : Index buffer (uint32) for triangle rendering.
     *
     * Notes:
     *   - Sets m_indexCount to indices.size() (drawElements path).
     */
    void uploadInterleavedPosNormalUVIndexed(const std::vector<float>& vertices,
                                             const std::vector<std::uint32_t>& indices);

    /*
     * Submits the mesh draw call.
     *
     * Behavior:
     *   - If m_indexCount > 0: draws using glDrawElements(GL_TRIANGLES, ...).
     *   - Else: draws using glDrawArrays(GL_TRIANGLES, ...).
     *
     * Notes:
     *   - Assumes an appropriate shader is bound and required uniforms/textures are set by the caller.
     */
    void draw() const;

private:
    // OpenGL object handles.
    GLuint  m_vao = 0;
    GLuint  m_vbo = 0;
    GLuint  m_ebo = 0;

    // Counts used for draw submission.
    GLsizei m_vertexCount = 0;
    GLsizei m_indexCount  = 0; // 0 => glDrawArrays, >0 => glDrawElements
};
