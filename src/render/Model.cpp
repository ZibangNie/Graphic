/*
 * Model.cpp
 *
 * Purpose:
 *   Implements Model, a minimal glTF (.glb) loader and renderer using tinygltf + OpenGL.
 *   Focuses on:
 *     - Loading binary glTF (GLB) geometry and material base color (albedo) textures
 *     - Building interleaved vertex buffers (pos/normal/uv) with optional index buffers
 *     - Drawing model parts using a provided shader with a small, fixed uniform interface
 *
 * Supported features (intentionally limited):
 *   - Mesh primitives with POSITION, optional NORMAL, optional TEXCOORD_0
 *   - BaseColorFactor and BaseColorTexture (PBR metallic-roughness base color only)
 *   - Node transforms via either a full 4x4 matrix or TRS (translation/rotation/scale)
 *
 * Non-goals / omitted features:
 *   - Skinning, animations, morph targets
 *   - Metallic/roughness, normal maps, emissive maps, IBL
 *   - Sampler state from glTF (wrap/filter are set to common defaults)
 *
 * Texture loading policy:
 *   - Disables tinygltf's stb integration and decodes embedded PNG/JPG bytes via the project's stb_image.
 *   - Forces stbi_set_flip_vertically_on_load(false) for glTF consistency.
 *
 * Shader interface expectation (see model.vert/model.frag):
 *   - uModel, uView, uProj
 *   - uBaseColorFactor (vec4)
 *   - uHasAlbedo (int), uAlbedo (sampler2D)
 */

#include "render/Model.h"

#include <iostream>
#include <unordered_map>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "stb_image.h"

// Disable tinygltf built-in stb implementation to avoid symbol duplication with stb_image_impl.cpp.
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

/*
 * Constructs a node local transform matrix from glTF node data.
 *
 * Parameters:
 *   n : tinygltf node.
 *
 * Returns:
 *   4x4 transform matrix. If n.matrix is present, uses it directly; otherwise composes TRS:
 *     M = T * R * S
 *
 * Notes:
 *   - glTF matrices are stored column-major; GLM is also column-major, but the indexing must match glTF's layout.
 *   - glTF quaternion is [x, y, z, w]; GLM expects (w, x, y, z).
 */
static glm::mat4 MatFromNodeTRS(const tinygltf::Node& n) {
    if (n.matrix.size() == 16) {
        glm::mat4 m(1.0f);
        // glTF matrix is column-major.
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                m[c][r] = (float)n.matrix[c * 4 + r];
        return m;
    }

    glm::vec3 t(0.0f);
    if (n.translation.size() == 3) t = glm::vec3((float)n.translation[0], (float)n.translation[1], (float)n.translation[2]);

    glm::vec3 s(1.0f);
    if (n.scale.size() == 3) s = glm::vec3((float)n.scale[0], (float)n.scale[1], (float)n.scale[2]);

    glm::quat q(1.0f, 0.0f, 0.0f, 0.0f);
    if (n.rotation.size() == 4) q = glm::quat((float)n.rotation[3], (float)n.rotation[0], (float)n.rotation[1], (float)n.rotation[2]); // (w,x,y,z)

    glm::mat4 M(1.0f);
    M = glm::translate(M, t) * glm::mat4_cast(q) * glm::scale(glm::mat4(1.0f), s);
    return M;
}

/*
 * Reads a glTF accessor as a float array and validates the expected component count.
 *
 * Parameters:
 *   model              : Parsed glTF model.
 *   accessorIndex      : Index of the accessor to read.
 *   expectedCompCount  : Expected component count per element (1/2/3/4).
 *   out                : Output float array resized to (count * expectedCompCount).
 *
 * Returns:
 *   true on success; false on validation/read failure.
 *
 * Notes:
 *   - Only FLOAT component type is supported for this helper.
 *   - Honors bufferView stride if specified; otherwise assumes tightly packed floats.
 */
static bool ReadAccessorFloat(const tinygltf::Model& model,
                              int accessorIndex,
                              int expectedCompCount,
                              std::vector<float>& out) {
    if (accessorIndex < 0) return false;

    const auto& acc = model.accessors[accessorIndex];
    const auto& bv  = model.bufferViews[acc.bufferView];
    const auto& buf = model.buffers[bv.buffer];

    if (acc.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
        std::cerr << "[GLB] Accessor componentType is not FLOAT\n";
        return false;
    }

    int compCount = 0;
    switch (acc.type) {
        case TINYGLTF_TYPE_SCALAR: compCount = 1; break;
        case TINYGLTF_TYPE_VEC2:   compCount = 2; break;
        case TINYGLTF_TYPE_VEC3:   compCount = 3; break;
        case TINYGLTF_TYPE_VEC4:   compCount = 4; break;
        default: break;
    }
    if (compCount != expectedCompCount) {
        std::cerr << "[GLB] Accessor type mismatch. expected=" << expectedCompCount << " got=" << compCount << "\n";
        return false;
    }

    const size_t stride = acc.ByteStride(bv) ? acc.ByteStride(bv) : (sizeof(float) * compCount);

    const size_t baseOffset = bv.byteOffset + acc.byteOffset;
    const unsigned char* base = buf.data.data() + baseOffset;

    out.resize((size_t)acc.count * (size_t)compCount);

    for (size_t i = 0; i < (size_t)acc.count; ++i) {
        const float* p = reinterpret_cast<const float*>(base + i * stride);
        for (int c = 0; c < compCount; ++c) out[i * compCount + c] = p[c];
    }
    return true;
}

/*
 * Reads a glTF index accessor and expands it to uint32 indices.
 *
 * Parameters:
 *   model         : Parsed glTF model.
 *   accessorIndex : Index accessor index.
 *   out           : Output indices as uint32.
 *
 * Returns:
 *   true on success; false on unsupported type or invalid input.
 *
 * Supported index component types:
 *   - UNSIGNED_BYTE, UNSIGNED_SHORT, UNSIGNED_INT
 */
static bool ReadIndicesU32(const tinygltf::Model& model, int accessorIndex, std::vector<std::uint32_t>& out) {
    out.clear();
    if (accessorIndex < 0) return false;

    const auto& acc = model.accessors[accessorIndex];
    const auto& bv  = model.bufferViews[acc.bufferView];
    const auto& buf = model.buffers[bv.buffer];

    const size_t baseOffset = bv.byteOffset + acc.byteOffset;
    const unsigned char* base = buf.data.data() + baseOffset;

    out.resize((size_t)acc.count);

    if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        const auto* p = reinterpret_cast<const std::uint16_t*>(base);
        for (size_t i = 0; i < out.size(); ++i) out[i] = (std::uint32_t)p[i];
        return true;
    }
    if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
        const auto* p = reinterpret_cast<const std::uint32_t*>(base);
        for (size_t i = 0; i < out.size(); ++i) out[i] = p[i];
        return true;
    }
    if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
        const auto* p = reinterpret_cast<const std::uint8_t*>(base);
        for (size_t i = 0; i < out.size(); ++i) out[i] = (std::uint32_t)p[i];
        return true;
    }

    std::cerr << "[GLB] Unsupported index componentType\n";
    return false;
}

/*
 * Generates vertex normals if the source primitive does not provide NORMAL attributes.
 *
 * Parameters:
 *   pos     : Positions array (3 floats per vertex).
 *   idx     : Triangle index array.
 *   normals : Output normals (3 floats per vertex), accumulated per face then normalized.
 *
 * Notes:
 *   - Uses simple area-unweighted face normals and per-vertex accumulation.
 *   - If a vertex ends up with near-zero accumulated normal, falls back to (0,1,0).
 */
static void GenerateNormalsIfMissing(const std::vector<float>& pos, const std::vector<std::uint32_t>& idx, std::vector<float>& normals) {
    const size_t vcount = pos.size() / 3;
    normals.assign(vcount * 3, 0.0f);

    for (size_t i = 0; i + 2 < idx.size(); i += 3) {
        std::uint32_t i0 = idx[i], i1 = idx[i+1], i2 = idx[i+2];
        if (i0 >= vcount || i1 >= vcount || i2 >= vcount) continue;

        glm::vec3 p0(pos[i0*3+0], pos[i0*3+1], pos[i0*3+2]);
        glm::vec3 p1(pos[i1*3+0], pos[i1*3+1], pos[i1*3+2]);
        glm::vec3 p2(pos[i2*3+0], pos[i2*3+1], pos[i2*3+2]);

        glm::vec3 n = glm::normalize(glm::cross(p1 - p0, p2 - p0));
        for (auto vi : {i0, i1, i2}) {
            normals[vi*3+0] += n.x;
            normals[vi*3+1] += n.y;
            normals[vi*3+2] += n.z;
        }
    }

    for (size_t v = 0; v < vcount; ++v) {
        glm::vec3 n(normals[v*3+0], normals[v*3+1], normals[v*3+2]);
        float len = glm::length(n);
        if (len > 1e-6f) n /= len;
        else n = glm::vec3(0,1,0);
        normals[v*3+0] = n.x;
        normals[v*3+1] = n.y;
        normals[v*3+2] = n.z;
    }
}

/*
 * Decodes an encoded image (PNG/JPG) stored in memory and creates an OpenGL 2D texture.
 *
 * Parameters:
 *   bytes     : Pointer to encoded image bytes.
 *   sizeBytes : Byte count.
 *
 * Returns:
 *   OpenGL texture handle, or 0 on failure.
 *
 * Texture defaults:
 *   - Wrap: GL_REPEAT
 *   - Min filter: GL_LINEAR_MIPMAP_LINEAR
 *   - Mag filter: GL_LINEAR
 *
 * Notes:
 *   - Forces stbi_set_flip_vertically_on_load(false) since glTF UV origin expects no vertical flip.
 *   - Uses the decoded component count to select GL_RED / GL_RGB / GL_RGBA.
 */
static GLuint CreateGLTextureFromEncodedImageBytes(const unsigned char* bytes, int sizeBytes) {
    if (!bytes || sizeBytes <= 0) return 0;

    // Disable vertical flip for glTF textures (independent of global stb settings elsewhere).
    stbi_set_flip_vertically_on_load(false);

    int w=0, h=0, comp=0;
    unsigned char* data = stbi_load_from_memory(bytes, sizeBytes, &w, &h, &comp, 0);
    if (!data) return 0;

    GLenum format = GL_RGB;
    if (comp == 1) format = GL_RED;
    else if (comp == 3) format = GL_RGB;
    else if (comp == 4) format = GL_RGBA;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    return tex;
}

/*
 * Destructor.
 *
 * Side effects:
 *   - Releases owned textures and clears loaded parts.
 */
Model::~Model() {
    clear();
}

/*
 * Clears all loaded model parts and deletes any OpenGL textures created during loading.
 *
 * Notes:
 *   - Textures are tracked in m_ownedTextures so the Model can manage their lifetime.
 */
void Model::clear() {
    for (GLuint t : m_ownedTextures) {
        if (t) glDeleteTextures(1, &t);
    }
    m_ownedTextures.clear();
    m_parts.clear();
}

/*
 * Loads a binary glTF model (.glb) from disk and builds drawable parts.
 *
 * Parameters:
 *   glbPath : File path to the .glb asset.
 *
 * Returns:
 *   true if at least one drawable primitive is loaded; false otherwise.
 *
 * Behavior overview:
 *   - Uses tinygltf to parse GLB without decoding images.
 *   - For each primitive:
 *       - Reads POSITION (required), NORMAL/UV (optional)
 *       - Reads indices (optional; generates sequential indices if missing)
 *       - Generates normals if missing
 *       - Creates an interleaved vertex buffer (pos3 + nrm3 + uv2) and uploads it to a Mesh
 *       - Extracts baseColorFactor and baseColorTexture (if present) and creates an OpenGL texture
 *       - Stores the primitive as a Model::Part with its local transform
 *
 * Notes:
 *   - Only base color textures are handled; other PBR textures are ignored.
 *   - Image decoding uses stb_image from memory (bufferView bytes).
 */
bool Model::loadFromGLB(const std::string& glbPath) {
    clear();

    tinygltf::Model gltf;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    // tinygltf requires an image loader callback when image loading is enabled.
    // Images are decoded manually from bufferView-encoded bytes, so this callback is a no-op.
    static auto DummyLoadImageData =
        [](tinygltf::Image* image, const int image_idx,
           std::string* err, std::string* warn,
           int req_width, int req_height,
           const unsigned char* bytes, int size,
           void* user_data) -> bool
        {
            // No decoding here; return true to continue parsing.
            return true;
        };

    loader.SetImageLoader(DummyLoadImageData, nullptr);

    bool ok = loader.LoadBinaryFromFile(&gltf, &err, &warn, glbPath);

    if (!warn.empty()) std::cerr << "[GLB] warn: " << warn << "\n";
    if (!err.empty())  std::cerr << "[GLB] err : " << err << "\n";
    if (!ok) {
        std::cerr << "[GLB] Failed to load: " << glbPath << "\n";
        return false;
    }

    std::cerr << "[GLB] meshes=" << gltf.meshes.size()
          << " nodes=" << gltf.nodes.size()
          << " images=" << gltf.images.size()
          << " materials=" << gltf.materials.size()
          << "\n";

    // Cache: glTF image index -> OpenGL texture handle.
    std::unordered_map<int, GLuint> imageTexCache;

    /*
     * Resolves base color texture + factor from a glTF material.
     *
     * Parameters:
     *   materialIndex : Index into gltf.materials.
     *   outFactor     : Output baseColorFactor (defaults to 1,1,1,1).
     *
     * Returns:
     *   OpenGL texture handle (0 if no base color texture).
     *
     * Notes:
     *   - Uses pbrMetallicRoughness.baseColorTexture and baseColorFactor.
     *   - Textures are created once per image and cached across materials/primitives.
     */
    auto getTextureFromMaterial = [&](int materialIndex, glm::vec4& outFactor) -> GLuint {
        outFactor = glm::vec4(1,1,1,1);
        if (materialIndex < 0 || materialIndex >= (int)gltf.materials.size()) return 0;

        const auto& mat = gltf.materials[materialIndex];
        const auto& pbr = mat.pbrMetallicRoughness;

        if (pbr.baseColorFactor.size() == 4) {
            outFactor = glm::vec4((float)pbr.baseColorFactor[0],
                                  (float)pbr.baseColorFactor[1],
                                  (float)pbr.baseColorFactor[2],
                                  (float)pbr.baseColorFactor[3]);
        }

        int texIndex = pbr.baseColorTexture.index;
        if (texIndex < 0 || texIndex >= (int)gltf.textures.size()) return 0;

        int imageIndex = gltf.textures[texIndex].source;
        if (imageIndex < 0 || imageIndex >= (int)gltf.images.size()) return 0;

        auto it = imageTexCache.find(imageIndex);
        if (it != imageTexCache.end()) return it->second;

        const auto& img = gltf.images[imageIndex];

        // When tinygltf does not decode, read encoded bytes from bufferView and decode via stb_image.
        GLuint tex = 0;
        if (img.bufferView >= 0 && img.bufferView < (int)gltf.bufferViews.size()) {
            const auto& bv  = gltf.bufferViews[img.bufferView];
            const auto& buf = gltf.buffers[bv.buffer];
            const unsigned char* bytes = buf.data.data() + bv.byteOffset;
            const int sizeBytes = (int)bv.byteLength;
            tex = CreateGLTextureFromEncodedImageBytes(bytes, sizeBytes);
        }

        if (tex) {
            imageTexCache[imageIndex] = tex;
            m_ownedTextures.push_back(tex);
        }
        return tex;
    };

    // Recursive scene graph traversal: accumulates parent transforms and emits primitives as Parts.
    std::function<void(int, const glm::mat4&)> processNode;
    processNode = [&](int nodeIndex, const glm::mat4& parent) {
        if (nodeIndex < 0 || nodeIndex >= (int)gltf.nodes.size()) return;
        const auto& node = gltf.nodes[nodeIndex];

        glm::mat4 local = MatFromNodeTRS(node);
        glm::mat4 cur   = parent * local;

        if (node.mesh >= 0 && node.mesh < (int)gltf.meshes.size()) {
            const auto& mesh = gltf.meshes[node.mesh];

            for (const auto& prim : mesh.primitives) {
                // Read POSITION (required), NORMAL/UV (optional).
                std::vector<float> pos, nrm, uv;
                auto itPos = prim.attributes.find("POSITION");
                if (itPos == prim.attributes.end()) continue;

                if (!ReadAccessorFloat(gltf, itPos->second, 3, pos)) continue;

                auto itN = prim.attributes.find("NORMAL");
                if (itN != prim.attributes.end()) ReadAccessorFloat(gltf, itN->second, 3, nrm);

                auto itUV = prim.attributes.find("TEXCOORD_0");
                if (itUV != prim.attributes.end()) ReadAccessorFloat(gltf, itUV->second, 2, uv);

                // Indices (optional): if missing, generate sequential indices.
                std::vector<std::uint32_t> idx;
                if (prim.indices >= 0) {
                    ReadIndicesU32(gltf, prim.indices, idx);
                } else {
                    const size_t vcount = pos.size() / 3;
                    idx.resize(vcount);
                    for (size_t i = 0; i < vcount; ++i) idx[i] = (std::uint32_t)i;
                }

                // If normals are missing, compute them from triangles.
                if (nrm.empty()) {
                    GenerateNormalsIfMissing(pos, idx, nrm);
                }

                // Build interleaved vertex buffer: pos3 + nrm3 + uv2 (UV defaults to 0 if absent).
                const size_t vcount = pos.size() / 3;
                std::vector<float> interleaved;
                interleaved.resize(vcount * 8, 0.0f);

                for (size_t v = 0; v < vcount; ++v) {
                    interleaved[v*8+0] = pos[v*3+0];
                    interleaved[v*8+1] = pos[v*3+1];
                    interleaved[v*8+2] = pos[v*3+2];

                    interleaved[v*8+3] = nrm[v*3+0];
                    interleaved[v*8+4] = nrm[v*3+1];
                    interleaved[v*8+5] = nrm[v*3+2];

                    if (!uv.empty()) {
                        interleaved[v*8+6] = uv[v*2+0];
                        interleaved[v*8+7] = uv[v*2+1];
                    } else {
                        interleaved[v*8+6] = 0.0f;
                        interleaved[v*8+7] = 0.0f;
                    }
                }

                Part part;
                part.local = cur;

                glm::vec4 factor(1,1,1,1);
                GLuint tex = getTextureFromMaterial(prim.material, factor);
                part.baseColorFactor = factor;
                part.albedoTex = tex;

                part.mesh.uploadInterleavedPosNormalUVIndexed(interleaved, idx);

                m_parts.emplace_back(std::move(part));
            }
        }

        for (int child : node.children) {
            processNode(child, cur);
        }
    };

    // Select scene: defaultScene if provided, otherwise scene 0.
    int sceneIndex = gltf.defaultScene >= 0 ? gltf.defaultScene : 0;
    if (sceneIndex < 0 || sceneIndex >= (int)gltf.scenes.size()) {
        std::cerr << "[GLB] No valid scene\n";
        return false;
    }

    glm::mat4 I(1.0f);
    const auto& scene = gltf.scenes[sceneIndex];
    for (int nodeIndex : scene.nodes) {
        processNode(nodeIndex, I);
    }

    if (m_parts.empty()) {
        std::cerr << "[GLB] Loaded but no drawable primitives: " << glbPath << "\n";
        return false;
    }

    std::cerr << "[GLB] loaded parts=" << m_parts.size() << "\n";

    return true;
}

/*
 * Draws the model using the provided shader and matrices.
 *
 * Parameters:
 *   shader      : Shader used for model rendering (expects model shader uniform interface).
 *   modelMatrix : Caller-provided model transform applied to the entire asset.
 *   view        : View matrix.
 *   proj        : Projection matrix.
 *
 * Behavior:
 *   - Sets uView/uProj once for the draw call.
 *   - For each Part:
 *       - Sets uModel = modelMatrix * part.local
 *       - Sets uBaseColorFactor
 *       - Binds base color texture if present and sets uHasAlbedo/uAlbedo
 *       - Draws the part mesh
 *
 * Notes:
 *   - Texture unit 0 is used for albedo.
 *   - Does not set clip plane uniforms; clipping (if needed) is handled by the caller/shader configuration.
 */
void Model::draw(Shader& shader,
                 const glm::mat4& modelMatrix,
                 const glm::mat4& view,
                 const glm::mat4& proj) const {
    shader.use();
    shader.setMat4("uView", view);
    shader.setMat4("uProj", proj);

    for (const auto& p : m_parts) {
        shader.setMat4("uModel", modelMatrix * p.local);
        shader.setVec4("uBaseColorFactor", p.baseColorFactor);

        if (p.albedoTex) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, p.albedoTex);
            shader.setInt("uAlbedo", 0);
            shader.setInt("uHasAlbedo", 1);
        } else {
            shader.setInt("uHasAlbedo", 0);
        }

        p.mesh.draw();

        // Unbind to avoid leaking state to subsequent draws that assume no texture bound on unit 0.
        if (p.albedoTex) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }
}
