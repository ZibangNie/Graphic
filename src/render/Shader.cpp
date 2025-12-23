/*
 * Shader.cpp
 *
 * Purpose:
 *   Implements Shader, a minimal OpenGL shader program wrapper that:
 *     - Loads GLSL source code from files
 *     - Compiles vertex/fragment shaders
 *     - Links them into a program object
 *     - Provides basic uniform setters for common types
 *
 * Design notes:
 *   - This implementation treats shader compilation/linking failures as fatal and terminates the program.
 *   - Uniform locations are queried on each set* call (simple and sufficient for small projects,
 *     but can be optimized by caching locations if needed).
 *
 * Conventions:
 *   - setMat4 uploads matrices with GL_FALSE (no transpose), matching GLM's column-major layout.
 */

#include "Shader.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>

/*
 * Destructor.
 *
 * Side effects:
 *   - Deletes the OpenGL program if created.
 *
 * Notes:
 *   - Requires a valid OpenGL context at destruction time.
 */
Shader::~Shader() {
    if (m_program) glDeleteProgram(m_program);
}

/*
 * Reads an entire text file into a std::string.
 *
 * Parameters:
 *   path : File system path to the text file.
 *
 * Returns:
 *   File contents as a string.
 *
 * Failure policy:
 *   - On failure to open the file, prints an error and terminates the program.
 */
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

/*
 * Compiles a single GLSL shader stage.
 *
 * Parameters:
 *   stage : OpenGL shader stage enum (e.g., GL_VERTEX_SHADER, GL_FRAGMENT_SHADER).
 *   src   : GLSL source code.
 *
 * Returns:
 *   OpenGL shader object handle.
 *
 * Failure policy:
 *   - On compilation error, prints the info log and terminates the program.
 */
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

/*
 * Links a vertex and fragment shader into a program.
 *
 * Parameters:
 *   vs : Vertex shader object handle.
 *   fs : Fragment shader object handle.
 *
 * Returns:
 *   OpenGL program object handle.
 *
 * Failure policy:
 *   - On link error, prints the program info log and terminates the program.
 *
 * Notes:
 *   - Detaches shaders after linking; ownership of shader objects remains with the caller.
 */
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

/*
 * Loads, compiles, and links a shader program from vertex/fragment shader files.
 *
 * Parameters:
 *   vertexPath   : Path to the vertex shader source file.
 *   fragmentPath : Path to the fragment shader source file.
 *
 * Returns:
 *   true on success (compilation/link errors are treated as fatal and will terminate).
 *
 * Notes:
 *   - Replaces any previously loaded program, deleting the old program handle.
 */
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

/*
 * Binds this shader program for subsequent draw calls.
 */
void Shader::use() const {
    glUseProgram(m_program);
}

/*
 * Sets a mat4 uniform.
 *
 * Parameters:
 *   name : Uniform name in the GLSL program.
 *   m    : Matrix value (column-major, GLM default).
 *
 * Notes:
 *   - Uploads with transpose = GL_FALSE to match GLM layout.
 */
void Shader::setMat4(const char* name, const glm::mat4& m) const {
    GLint loc = glGetUniformLocation(m_program, name);
    glUniformMatrix4fv(loc, 1, GL_FALSE, &m[0][0]);
}

/*
 * Sets a vec3 uniform.
 *
 * Parameters:
 *   name : Uniform name in the GLSL program.
 *   v    : Vector value.
 */
void Shader::setVec3(const char* name, const glm::vec3& v) const {
    GLint loc = glGetUniformLocation(m_program, name);
    glUniform3fv(loc, 1, &v[0]);
}

/*
 * Sets a float uniform.
 *
 * Parameters:
 *   name : Uniform name in the GLSL program.
 *   f    : Scalar value.
 */
void Shader::setFloat(const char* name, float f) const {
    GLint loc = glGetUniformLocation(m_program, name);
    glUniform1f(loc, f);
}

/*
 * Sets an int uniform.
 *
 * Parameters:
 *   name : Uniform name in the GLSL program.
 *   i    : Integer value (commonly used for sampler bindings or feature toggles).
 */
void Shader::setInt(const char* name, int i) const {
    GLint loc = glGetUniformLocation(m_program, name);
    glUniform1i(loc, i);
}

/*
 * Sets a vec4 uniform.
 *
 * Parameters:
 *   name : Uniform name in the GLSL program.
 *   v    : Vector value.
 */
void Shader::setVec4(const char* name, const glm::vec4& v) const {
    GLint loc = glGetUniformLocation(m_program, name);
    glUniform4fv(loc, 1, &v[0]);
}
