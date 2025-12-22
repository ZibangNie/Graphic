#pragma once
#include <string>
#include <filesystem>
#include <glad/glad.h>
#include <glm/glm.hpp>

#include "scene/Camera.h"
#include "Environment.h"
#include "render/Shader.h"

class Sky {
public:
    bool init(const std::filesystem::path& assetsRoot,
              const std::string& dayHdrRel,
              const std::string& nightHdrRel,
              int cubemapSize = 512);

    void render(const Camera& camera,
                const glm::mat4& proj,
                const Environment& env);

    void shutdown();

private:
    GLuint m_dayCube = 0;
    GLuint m_nightCube = 0;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;

    Shader m_shader;
    bool m_ready = false;

    void createCube();
};
