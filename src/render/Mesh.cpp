#include "Mesh.h"
#include <iostream>

Mesh::~Mesh() {
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
}

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

    // layout(location=0): pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // layout(location=1): color
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

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

    // layout(location=0): pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // layout(location=1): normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // layout(location=2): uv
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

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

    // layout(location=0): pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // layout(location=1): normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // layout(location=2): uv
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Mesh::draw() const {
    glBindVertexArray(m_vao);
    if (m_indexCount > 0) {
        glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, (void*)0);
    } else {
        glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
    }
    glBindVertexArray(0);
}
