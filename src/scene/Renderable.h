#pragma once

#include "Render/Mesh.h"
#include "Render/Shader.h"
#include "Scene/Transform.h"

#include <glm/glm.hpp>

struct Renderable {
    Mesh* mesh = nullptr;
    Shader* shader = nullptr;
    Transform transform;

    void draw(const glm::mat4& view, const glm::mat4& proj) const {
        if (!mesh || !shader) return;

        shader->use();
        shader->setMat4("uModel", transform.modelMatrix());
        shader->setMat4("uView", view);
        shader->setMat4("uProj", proj);
        mesh->draw();
    }
};
