#pragma once
#include <string>
#include <vector>
#include <cstdint>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "render/Mesh.h"
#include "render/Shader.h"

class Model {
public:
    Model() = default;
    ~Model();

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    Model(Model&&) noexcept = default;
    Model& operator=(Model&&) noexcept = default;

    bool loadFromGLB(const std::string& glbPath);

    // 外部传入 view/proj，保持你 main.cpp 的组织方式
    void draw(Shader& shader,
              const glm::mat4& modelMatrix,
              const glm::mat4& view,
              const glm::mat4& proj) const;

private:
    struct Part {
        Mesh mesh;
        GLuint albedoTex = 0;                 // 0 => no texture
        glm::vec4 baseColorFactor{1,1,1,1};   // fallback / multiply
        glm::mat4 local{1.0f};                // node transform
    };

    std::vector<Part>   m_parts;
    std::vector<GLuint> m_ownedTextures;

    void clear();
};
