#pragma once
#include <vector>
#include <cstdint>
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

    // Vertex format: pos(3) + normal(3) + uv(2), stride = 8 floats
    void uploadInterleavedPosNormalUV(const std::vector<float>& vertices);

    // Vertex format: pos(3) + normal(3) + uv(2), stride = 8 floats, indexed draw
    void uploadInterleavedPosNormalUVIndexed(const std::vector<float>& vertices,
                                             const std::vector<std::uint32_t>& indices);

    void draw() const;

private:
    GLuint  m_vao = 0;
    GLuint  m_vbo = 0;
    GLuint  m_ebo = 0;

    GLsizei m_vertexCount = 0;
    GLsizei m_indexCount  = 0; // 0 => draw arrays, >0 => draw elements
};
