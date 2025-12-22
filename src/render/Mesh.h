#pragma once
#include <vector>
#include <glad/glad.h>

class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    // Vertex format: pos(3) + color(3), stride = 6 floats
    void uploadInterleavedPosColor(const std::vector<float>& vertices);

    void draw() const;

private:
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLsizei m_vertexCount = 0;
};
