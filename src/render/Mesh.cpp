#include "Mesh.h"
#include <cstdlib>
#include <iostream>

Mesh::~Mesh() {
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
}

Mesh::Mesh(Mesh&& other) noexcept {
    m_vao = other.m_vao;
    m_vbo = other.m_vbo;
    m_vertexCount = other.m_vertexCount;

    other.m_vao = 0;
    other.m_vbo = 0;
    other.m_vertexCount = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this == &other) return *this;

    // release current
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);

    // steal
    m_vao = other.m_vao;
    m_vbo = other.m_vbo;
    m_vertexCount = other.m_vertexCount;

    other.m_vao = 0;
    other.m_vbo = 0;
    other.m_vertexCount = 0;

    return *this;
}

void Mesh::uploadInterleavedPosColor(const std::vector<float>& vertices) {
    if (vertices.empty() || vertices.size() % 6 != 0) {
        std::cerr << "[Mesh] Invalid vertex data (need multiples of 6 floats)\n";
        std::exit(EXIT_FAILURE);
    }

    m_vertexCount = static_cast<GLsizei>(vertices.size() / 6);

    if (!m_vao) glGenVertexArrays(1, &m_vao);
    if (!m_vbo) glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(vertices.size() * sizeof(float)), vertices.data(), GL_STATIC_DRAW);

    // layout(location=0): position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // layout(location=1): color
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Mesh::draw() const {
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
    glBindVertexArray(0);
}
