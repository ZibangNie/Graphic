#pragma once
#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>

class Shader {
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);

    void use() const;
    GLuint id() const { return m_program; }

    // Uniform helpers (minimal set)
    void setMat4(const char* name, const glm::mat4& m) const;
    void setVec3(const char* name, const glm::vec3& v) const;
    void setFloat(const char* name, float f) const;
    void setInt(const char* name, int i) const;
    void setVec4(const char* name, const glm::vec4& v) const;


private:
    GLuint m_program = 0;

    static std::string readTextFile(const std::string& path);
    static GLuint compile(GLenum stage, const std::string& src);
    static GLuint link(GLuint vs, GLuint fs);
};
