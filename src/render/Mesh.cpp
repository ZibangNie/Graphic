/*
 * Mesh.cpp
 *
 * Purpose:
 *   Implements the Mesh wrapper for OpenGL vertex array/buffer management and draw submission.
 *   Supports:
 *     - Non-indexed meshes with interleaved vertex formats
 *     - Indexed meshes with an element buffer
 *
 * Vertex formats supported (interleaved, tightly packed):
 *   1) Pos + Color:
 *      - 6 floats per vertex: [px, py, pz, cr, cg, cb]
 *      - Attribute locations: 0=pos (vec3), 1=color (vec3)
 *
 *   2) Pos + Normal + UV:
 *      - 8 floats per vertex: [px, py, pz, nx, ny, nz, u, v]
 *      - Attribute locations: 0=pos (vec3), 1=normal (vec3), 2=uv (vec2)
 *
 * Ownership / lifetime:
 *   - Mesh owns its VAO/VBO/EBO handles and deletes them in the destructor.
 *   - Copy is typically disabled in the header (not shown here); move is supported via move ctor/assign.
 */

#include "Mesh.h"
#include <iostream>

/*
 * Destructor.
 *
 * Side effects:
 *   - Deletes OpenGL buffers/VAO if they were created.
 *
 * Notes:
 *   - OpenGL context must still be valid when the destructor runs.
 */
Mesh::~Mesh() {
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
}

/*
 * Move constructor.
 *
 * Parameters:
 *   other : Mesh to move from.
 *
 * Behavior:
 *   - Transfers ownership of GL handles and counts from other.
 *   - Resets other to a safe, empty state (handles = 0, counts = 0).
 */
Mesh::Mesh(Mesh&& other) noexcept {
    m_vao = other.m_vao;
    m_vbo = other.m_vbo;
    m_ebo = other.m_ebo;
    m_vertexCount = other.m_vertexCount;
    m_indexCount  = other.m_indexCount;

    other.m_vao = 0;
    other.m_vbo = 0;
    other.m_ebo = 0;
    other.m_vertexCount = 0;
    other.m_indexCount  = 0;
}

/*
 * Move assignment operator.
 *
 * Parameters:
 *   other : Mesh to move from.
 *
 * Returns:
 *   Reference to this.
 *
 * Behavior:
 *   - Releases any currently owned GL handles.
 *   - Transfers ownership of GL handles and counts from other.
 *   - Resets other to a safe, empty state.
 */
Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this == &other) return *this;

    if (m_ebo) glDeleteBuffers(1, &m_ebo);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);

    m_vao = other.m_vao;
    m_vbo = other.m_vbo;
    m_ebo = other.m_ebo;
    m_vertexCount = other.m_vertexCount;
    m_indexCount  = other.m_indexCount;

    other.m_vao = 0;
    other.m_vbo = 0;
    other.m_ebo = 0;
    other.m_vertexCount = 0;
    other.m_indexCount  = 0;

    return *this;
}

/*
 * Uploads a non-indexed mesh with interleaved position + color attributes.
 *
 * Parameters:
 *   vertices : Interleaved vertex data. Expected layout per vertex:
 *              [px, py, pz, cr, cg, cb] (6 floats).
 *
 * Behavior:
 *   - Creates VAO/VBO if needed; deletes EBO if previously allocated (switching to non-indexed draw).
 *   - Uploads data to GL_ARRAY_BUFFER with GL_STATIC_DRAW usage.
 *   - Configures attributes:
 *       location 0: vec3 position
 *       location 1: vec3 color
 *
 * Notes:
 *   - m_vertexCount is derived as vertices.size() / 6.
 */
void Mesh::uploadInterleavedPosColor(const std::vector<float>& vertices) {
    if (vertices.empty()) return;
    m_vertexCount = (GLsizei)(vertices.size() / 6);
    m_indexCount  = 0;

    if (!m_vao) glGenVertexArrays(1, &m_vao);
    if (!m_vbo) glGenBuffers(1, &m_vbo);
    if (m_ebo) { glDeleteBuffers(1, &m_ebo); m_ebo = 0; }

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(vertices.size() * sizeof(float)), vertices.data(), GL_STATIC_DRAW);

    // layout(location=0): position (vec3)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // layout(location=1): color (vec3)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

/*
 * Uploads a non-indexed mesh with interleaved position + normal + UV attributes.
 *
 * Parameters:
 *   vertices : Interleaved vertex data. Expected layout per vertex:
 *              [px, py, pz, nx, ny, nz, u, v] (8 floats).
 *
 * Behavior:
 *   - Creates VAO/VBO if needed; deletes EBO if previously allocated.
 *   - Uploads data to GL_ARRAY_BUFFER with GL_STATIC_DRAW usage.
 *   - Configures attributes:
 *       location 0: vec3 position
 *       location 1: vec3 normal
 *       location 2: vec2 uv
 *
 * Notes:
 *   - m_vertexCount is derived as vertices.size() / 8.
 */
void Mesh::uploadInterleavedPosNormalUV(const std::vector<float>& vertices) {
    if (vertices.empty()) return;
    m_vertexCount = (GLsizei)(vertices.size() / 8);
    m_indexCount  = 0;

    if (!m_vao) glGenVertexArrays(1, &m_vao);
    if (!m_vbo) glGenBuffers(1, &m_vbo);
    if (m_ebo) { glDeleteBuffers(1, &m_ebo); m_ebo = 0; }

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(vertices.size() * sizeof(float)), vertices.data(), GL_STATIC_DRAW);

    // layout(location=0): position (vec3)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // layout(location=1): normal (vec3)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // layout(location=2): uv (vec2)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

/*
 * Uploads an indexed mesh with interleaved position + normal + UV attributes.
 *
 * Parameters:
 *   vertices : Interleaved vertex data. Expected layout per vertex:
 *              [px, py, pz, nx, ny, nz, u, v] (8 floats).
 *   indices  : Triangle indices (uint32). Drawn with glDrawElements(GL_TRIANGLES, ...).
 *
 * Behavior:
 *   - Creates VAO/VBO/EBO if needed.
 *   - Uploads vertex data to GL_ARRAY_BUFFER and index data to GL_ELEMENT_ARRAY_BUFFER.
 *   - Configures attributes:
 *       location 0: vec3 position
 *       location 1: vec3 normal
 *       location 2: vec2 uv
 *
 * Notes:
 *   - m_indexCount is set to indices.size().
 *   - GL_ELEMENT_ARRAY_BUFFER binding is stored in the VAO state; it remains bound to the VAO after setup.
 */
void Mesh::uploadInterleavedPosNormalUVIndexed(const std::vector<float>& vertices,
                                               const std::vector<std::uint32_t>& indices) {
    if (vertices.empty() || indices.empty()) return;

    m_vertexCount = (GLsizei)(vertices.size() / 8);
    m_indexCount  = (GLsizei)indices.size();

    if (!m_vao) glGenVertexArrays(1, &m_vao);
    if (!m_vbo) glGenBuffers(1, &m_vbo);
    if (!m_ebo) glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(vertices.size() * sizeof(float)), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indices.size() * sizeof(std::uint32_t)), indices.data(), GL_STATIC_DRAW);

    // layout(location=0): position (vec3)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // layout(location=1): normal (vec3)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // layout(location=2): uv (vec2)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

/*
 * Submits the mesh draw call.
 *
 * Behavior:
 *   - Binds the VAO.
 *   - If an index buffer is present (m_indexCount > 0), draws with glDrawElements().
 *     Otherwise, draws with glDrawArrays().
 *   - Unbinds the VAO after submission.
 *
 * Notes:
 *   - Assumes the caller has already bound the appropriate shader and set required uniforms/textures.
 */
void Mesh::draw() const {
    glBindVertexArray(m_vao);
    if (m_indexCount > 0) {
        glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, (void*)0);
    } else {
        glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
    }
    glBindVertexArray(0);
}
