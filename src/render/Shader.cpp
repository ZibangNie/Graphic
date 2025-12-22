#include "Shader.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>


Shader::~Shader() {
    if (m_program) glDeleteProgram(m_program);
}

std::string Shader::readTextFile(const std::string& path) {
    std::ifstream in(path, std::ios::in);
    if (!in) {
        std::cerr << "[Shader] Failed to open file: " << path << "\n";
        std::exit(EXIT_FAILURE);
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

GLuint Shader::compile(GLenum stage, const std::string& src) {
    GLuint sh = glCreateShader(stage);
    const char* c = src.c_str();
    glShaderSource(sh, 1, &c, nullptr);
    glCompileShader(sh);

    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetShaderInfoLog(sh, len, nullptr, log.data());

        std::cerr << "[Shader] Compile failed stage=" << stage << "\n" << log << "\n";
        glDeleteShader(sh);
        std::exit(EXIT_FAILURE);
    }
    return sh;
}

GLuint Shader::link(GLuint vs, GLuint fs) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetProgramInfoLog(prog, len, nullptr, log.data());

        std::cerr << "[Shader] Link failed\n" << log << "\n";
        glDeleteProgram(prog);
        std::exit(EXIT_FAILURE);
    }

    glDetachShader(prog, vs);
    glDetachShader(prog, fs);
    return prog;
}

bool Shader::loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }

    const auto vsSrc = readTextFile(vertexPath);
    const auto fsSrc = readTextFile(fragmentPath);

    GLuint vs = compile(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsSrc);
    m_program = link(vs, fs);

    glDeleteShader(vs);
    glDeleteShader(fs);
    return true;
}

void Shader::use() const {
    glUseProgram(m_program);
}

void Shader::setMat4(const char* name, const glm::mat4& m) const {
    GLint loc = glGetUniformLocation(m_program, name);
    glUniformMatrix4fv(loc, 1, GL_FALSE, &m[0][0]);
}

void Shader::setVec3(const char* name, const glm::vec3& v) const {
    GLint loc = glGetUniformLocation(m_program, name);
    glUniform3fv(loc, 1, &v[0]);
}

void Shader::setFloat(const char* name, float f) const {
    GLint loc = glGetUniformLocation(m_program, name);
    glUniform1f(loc, f);
}

void Shader::setInt(const char* name, int i) const {
    GLint loc = glGetUniformLocation(m_program, name);
    glUniform1i(loc, i);
}
