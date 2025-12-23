/*
 * Shader.h
 *
 * Purpose:
 *   Declares Shader, a minimal OpenGL shader program wrapper responsible for:
 *     - Loading GLSL vertex/fragment sources from files
 *     - Compiling and linking a program object
 *     - Binding the program and setting a small set of common uniform types
 *
 * Ownership / lifetime:
 *   - Owns a single OpenGL program handle (m_program) and deletes it in the destructor.
 *   - Copy is disabled (resource ownership); instances are intended to be managed by value or by pointer.
 *
 * Notes:
 *   - Uniform setters query locations by name on each call (simple, not optimized).
 *   - setMat4 uploads with transpose = GL_FALSE, matching GLM's column-major matrix layout.
 */

#pragma once
#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>

class Shader {
public:
    Shader() = default;

    /*
     * Destructor.
     *
     * Side effects:
     *   - Deletes the OpenGL program if created.
     */
    ~Shader();

    // Non-copyable: owns an OpenGL program handle.
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    /*
     * Loads, compiles, and links a shader program from two source files.
     *
     * Parameters:
     *   vertexPath   : Path to the vertex shader source.
     *   fragmentPath : Path to the fragment shader source.
     *
     * Returns:
     *   true on success (compilation/link failures are handled internally by the implementation).
     *
     * Notes:
     *   - Replaces any previously loaded program.
     */
    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);

    /*
     * Binds the shader program for subsequent uniform updates and draw calls.
     */
    void use() const;

    // Returns the raw OpenGL program handle.
    GLuint id() const { return m_program; }

    // Uniform helpers (minimal set).
    // Notes:
    //   - These functions upload to the currently owned program (m_program).
    //   - Uniform names must match the GLSL source exactly.
    void setMat4(const char* name, const glm::mat4& m) const;
    void setVec3(const char* name, const glm::vec3& v) const;
    void setFloat(const char* name, float f) const;
    void setInt(const char* name, int i) const;
    void setVec4(const char* name, const glm::vec4& v) const;

private:
    // OpenGL program object handle.
    GLuint m_program = 0;

    /*
     * Utility: reads a text file fully into a std::string.
     */
    static std::string readTextFile(const std::string& path);

    /*
     * Utility: compiles a GLSL shader stage.
     *
     * Parameters:
     *   stage : GL_VERTEX_SHADER / GL_FRAGMENT_SHADER, etc.
     *   src   : GLSL source code.
     */
    static GLuint compile(GLenum stage, const std::string& src);

    /*
     * Utility: links a program from compiled shader stage objects.
     *
     * Parameters:
     *   vs : Compiled vertex shader handle.
     *   fs : Compiled fragment shader handle.
     */
    static GLuint link(GLuint vs, GLuint fs);
};
