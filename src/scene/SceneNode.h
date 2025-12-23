/*
 * SceneNode.h
 *
 * Purpose:
 *   Declares a lightweight scene-graph node with hierarchical transforms and optional rendering.
 *   Each node:
 *     - Owns a local Transform (with parent linkage for hierarchical world transforms)
 *     - Optionally references a Mesh and Shader for rendering
 *     - Stores per-node parameters used by specific shaders (tint, terrain material bindings)
 *     - Owns child nodes via std::unique_ptr (tree ownership)
 *
 * Rendering model:
 *   - drawRecursive(view, proj) performs a depth-first traversal.
 *   - If mesh and shader are set, the node is rendered using:
 *       uModel = transform.worldMatrix()
 *       uView  = view
 *       uProj  = proj
 *       uTint  = tint  (optional; ignored if uniform not present)
 *
 * Terrain material convention (when applicable):
 *   - tex0/tex1 are bound to texture units 0/1 and mapped to uRocky/uSand.
 *   - uUVScale, uSandHeight, uBlendWidth are provided for height-based blending.
 *   - These uniforms are set unconditionally here; shaders that do not declare them
 *     will ignore the calls if locations are -1.
 *
 * Ownership / lifetime:
 *   - Children are owned by the parent (unique_ptr).
 *   - mesh/shader are non-owning pointers; external lifetime management is required.
 */

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

    // Optional render component (non-owning pointers).
    Mesh* mesh = nullptr;
    Shader* shader = nullptr;

    // Terrain material inputs (used by terrain shader; ignored by others if uniforms are absent).
    GLuint tex0 = 0; // rocky (texture unit 0)
    GLuint tex1 = 0; // sand  (texture unit 1)
    float uvScale = 0.05f;         // World-space UV scale (typical range ~0.01 to 0.2).
    float sandHeight = -0.5f;      // Height threshold: below tends toward sand.
    float blendWidth = 0.3f;       // Blend transition width (world units).

    // Per-node tint (requires shader uniform uTint; default white means "no tint").
    glm::vec3 tint{1.f, 1.f, 1.f};

    SceneNode() = default;
    explicit SceneNode(std::string n) : name(std::move(n)) {}

    /*
     * Adds a child node to this node and links transform parenting.
     *
     * Parameters:
     *   child : Unique ownership of the child node.
     *
     * Returns:
     *   Raw pointer to the newly added child (owned by this node).
     *
     * Notes:
     *   - The child's Transform parent is set to this node's Transform.
     */
    SceneNode* addChild(std::unique_ptr<SceneNode> child) {
        child->transform.setParent(&this->transform);
        m_children.emplace_back(std::move(child));
        return m_children.back().get();
    }

    /*
     * Child accessors (bounds-checked via std::vector::at).
     *
     * Parameters:
     *   i : Child index.
     *
     * Returns:
     *   Pointer to child node.
     */
    SceneNode* childAt(size_t i) { return m_children.at(i).get(); }
    const SceneNode* childAt(size_t i) const { return m_children.at(i).get(); }

    /*
     * Recursively renders this node and all descendants (depth-first traversal).
     *
     * Parameters:
     *   view : View matrix (world -> view space).
     *   proj : Projection matrix.
     *
     * Notes:
     *   - Renders only if both mesh and shader are assigned.
     *   - Terrain-related bindings are performed for every rendered node; shaders that do not
     *     declare these uniforms will effectively ignore them.
     *   - Texture units 0 and 1 are reset to 0 after drawing to reduce accidental state leakage.
     */
    void drawRecursive(const glm::mat4& view, const glm::mat4& proj) const {
        if (mesh && shader) {
            shader->use();
            shader->setMat4("uModel", transform.worldMatrix());
            shader->setMat4("uView", view);
            shader->setMat4("uProj", proj);
            shader->setVec3("uTint", tint); // If uTint is absent, OpenGL ignores location=-1.

            // --- Terrain material binding ---
            {
                // Bind textures even if an id is 0; recommended to provide valid fallbacks upstream.
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

            // Reset commonly used texture units to reduce accidental cross-draw coupling.
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
    // Owned children (scene-graph ownership).
    std::vector<std::unique_ptr<SceneNode>> m_children;
};
