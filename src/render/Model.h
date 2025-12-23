/*
 * Model.h
 *
 * Purpose:
 *   Declares Model, a minimal drawable asset container loaded from binary glTF (.glb).
 *   The Model stores a list of drawable Parts, each containing:
 *     - Mesh geometry (pos/normal/uv, indexed)
 *     - Optional base color (albedo) texture
 *     - BaseColorFactor multiplier (fallback and/or tint)
 *     - Local node transform for scene-graph placement within the asset
 *
 * Scope:
 *   - Intended for static glTF assets with basic PBR base color support.
 *   - Advanced glTF features (skinning, animation, morph targets, full PBR stack) are out of scope.
 *
 * Ownership / lifetime:
 *   - Owns OpenGL textures created during loading (tracked in m_ownedTextures).
 *   - Owns Mesh instances per Part (each Mesh manages its own VAO/VBO/EBO).
 *   - Copy is disabled (resource ownership). Move is supported.
 *
 * Shader interface expectation (typical):
 *   - uModel, uView, uProj
 *   - uBaseColorFactor (vec4)
 *   - uHasAlbedo (int), uAlbedo (sampler2D)
 */

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

    /*
     * Destructor.
     *
     * Side effects:
     *   - Releases textures created during loading and clears loaded parts.
     */
    ~Model();

    // Non-copyable: owns OpenGL resources through textures and Mesh handles.
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    // Movable: transfers ownership of parts and textures.
    Model(Model&&) noexcept = default;
    Model& operator=(Model&&) noexcept = default;

    /*
     * Loads a binary glTF model (.glb) from disk.
     *
     * Parameters:
     *   glbPath : File path to the .glb asset.
     *
     * Returns:
     *   true if at least one drawable primitive is loaded; false otherwise.
     *
     * Notes:
     *   - Loading creates OpenGL textures for base color images when present.
     *   - Geometry is stored as indexed pos/normal/uv meshes.
     */
    bool loadFromGLB(const std::string& glbPath);

    /*
     * Draws the model using the provided shader and matrices.
     *
     * Parameters:
     *   shader      : Shader used for model rendering.
     *   modelMatrix : Transform applied to the entire asset (external placement/scale/rotation).
     *   view        : View matrix (external; aligns with main render loop organization).
     *   proj        : Projection matrix (external; aligns with main render loop organization).
     *
     * Notes:
     *   - Each Part applies an additional local node transform: uModel = modelMatrix * part.local.
     *   - Albedo texture is bound on a fixed texture unit (commonly unit 0 in the implementation).
     */
    void draw(Shader& shader,
              const glm::mat4& modelMatrix,
              const glm::mat4& view,
              const glm::mat4& proj) const;

private:
    /*
     * A drawable primitive extracted from a glTF mesh.
     *
     * Fields:
     *   mesh            : Indexed geometry in pos/normal/uv format.
     *   albedoTex       : Base color texture handle (0 indicates no texture).
     *   baseColorFactor : Fallback/multiply color factor from glTF material (RGBA).
     *   local           : Accumulated node transform placing this primitive within the asset hierarchy.
     */
    struct Part {
        Mesh mesh;
        GLuint albedoTex = 0;                 // 0 => no texture
        glm::vec4 baseColorFactor{1,1,1,1};   // fallback / multiply
        glm::mat4 local{1.0f};                // node transform
    };

    // Loaded drawable parts.
    std::vector<Part>   m_parts;

    // OpenGL textures created and owned by this Model instance (for deletion on clear/destruction).
    std::vector<GLuint> m_ownedTextures;

    /*
     * Clears loaded content and deletes owned textures.
     *
     * Notes:
     *   - Called internally by loadFromGLB() and the destructor.
     */
    void clear();
};
