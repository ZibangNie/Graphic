#pragma once

#include <memory>
#include <vector>
#include <string>

#include <glm/glm.hpp>

#include "scene/Transform.h"
#include "render/Mesh.h"
#include "render/Shader.h"

class SceneNode {
public:
    std::string name;
    Transform transform;

    // Optional render component
    Mesh* mesh = nullptr;
    Shader* shader = nullptr;

    GLuint tex0 = 0; // rocky
    GLuint tex1 = 0; // sand
    float uvScale = 0.05f;
    float sandHeight = -0.5f;    // 低处为 sand
    float blendWidth = 0.3f;

    // Per-node tint (requires shader uniform uTint; default white)
    glm::vec3 tint{1.f, 1.f, 1.f};

    SceneNode() = default;
    explicit SceneNode(std::string n) : name(std::move(n)) {}

    SceneNode* addChild(std::unique_ptr<SceneNode> child) {
        child->transform.setParent(&this->transform);
        m_children.emplace_back(std::move(child));
        return m_children.back().get();
    }

    SceneNode* childAt(size_t i) { return m_children.at(i).get(); }
    const SceneNode* childAt(size_t i) const { return m_children.at(i).get(); }

    void drawRecursive(const glm::mat4& view, const glm::mat4& proj) const {
        if (mesh && shader) {
            shader->use();
            shader->setMat4("uModel", transform.worldMatrix());
            shader->setMat4("uView", view);
            shader->setMat4("uProj", proj);
            shader->setVec3("uTint", tint); // 若 shader 没有 uTint，location=-1 会被 OpenGL 忽略

            // --- Terrain material binding ---
            {
                // 即使某张纹理是 0，也绑定到对应 unit（但建议 main.cpp 已经提供 fallback，避免 0）
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, tex0);
                shader->setInt("uRocky", 0);

                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, tex1);
                shader->setInt("uSand", 1);

                shader->setFloat("uUVScale", uvScale);
                shader->setFloat("uSandHeight", sandHeight);
                shader->setFloat("uBlendWidth", blendWidth);
            }

            mesh->draw();

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        for (const auto& c : m_children) {
            c->drawRecursive(view, proj);
        }
    }

private:
    std::vector<std::unique_ptr<SceneNode>> m_children;
};
