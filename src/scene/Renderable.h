/*
* Renderable.h
 *
 * Purpose:
 *   Defines a minimal renderable object wrapper:
 *     - References a Mesh and a Shader (non-owning pointers)
 *     - Owns a local Transform used to produce the model matrix
 *     - Provides a convenience draw() that binds common matrices and issues the draw call
 *
 * Conventions:
 *   - Shader is expected to use the uniform names:
 *       uModel : mat4 model transform
 *       uView  : mat4 view transform
 *       uProj  : mat4 projection transform
 *
 * Ownership:
 *   - mesh and shader are non-owning; their lifetimes must exceed Renderable usage.
 */

#pragma once

#include "Render/Mesh.h"
#include "Render/Shader.h"
#include "Scene/Transform.h"

#include <glm/glm.hpp>

struct Renderable {
    Mesh* mesh = nullptr;         // Geometry source (non-owning).
    Shader* shader = nullptr;     // Program wrapper (non-owning).
    Transform transform;          // Local transform for computing the model matrix.

    /*
     * Draws the renderable using the provided view/projection matrices.
     *
     * Parameters:
     *   view : View matrix (world -> view space).
     *   proj : Projection matrix.
     *
     * Notes:
     *   - Returns early if mesh or shader is not assigned.
     *   - Only sets the common matrix uniforms; material-specific uniforms must be set elsewhere.
     */
    void draw(const glm::mat4& view, const glm::mat4& proj) const {
        if (!mesh || !shader) return;

        shader->use();
        shader->setMat4("uModel", transform.modelMatrix());
        shader->setMat4("uView", view);
        shader->setMat4("uProj", proj);
        mesh->draw();
    }
};
