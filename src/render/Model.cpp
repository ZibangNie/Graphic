#include "render/Model.h"

#include <iostream>
#include <unordered_map>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "stb_image.h"

// 禁用 tinygltf 内置 stb 实现，避免你现有 stb_image_impl.cpp 重复定义
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

static glm::mat4 MatFromNodeTRS(const tinygltf::Node& n) {
    if (n.matrix.size() == 16) {
        glm::mat4 m(1.0f);
        // glTF matrix is column-major
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

static GLuint CreateGLTextureFromEncodedImageBytes(const unsigned char* bytes, int sizeBytes) {
    if (!bytes || sizeBytes <= 0) return 0;

    // 你的 main.cpp 会把 flip 设成 true；glTF 通常不希望 flip，所以这里强制关掉再恢复
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

Model::~Model() {
    clear();
}

void Model::clear() {
    for (GLuint t : m_ownedTextures) {
        if (t) glDeleteTextures(1, &t);
    }
    m_ownedTextures.clear();
    m_parts.clear();
}

bool Model::loadFromGLB(const std::string& glbPath) {
    clear();

    tinygltf::Model gltf;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    // 必须提供 LoadImageData 回调，否则会报 "No LoadImageData callback specified."
    static auto DummyLoadImageData =
        [](tinygltf::Image* image, const int image_idx,
           std::string* err, std::string* warn,
           int req_width, int req_height,
           const unsigned char* bytes, int size,
           void* user_data) -> bool
        {
            // 我们自己后面会从 bufferView 拿编码图片字节并用 stb 解码创建 OpenGL 纹理，
            // 所以这里不需要 tinygltf 解码。返回 true 让解析继续。
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

    // 预先把 image->GL texture 做缓存（texture index -> GL id）
    std::unordered_map<int, GLuint> imageTexCache;

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

        // 如果 tinygltf 没解码（因为我们禁用 stb），从 bufferView 拿到 PNG/JPG 编码字节后自己 decode
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

    std::function<void(int, const glm::mat4&)> processNode;
    processNode = [&](int nodeIndex, const glm::mat4& parent) {
        if (nodeIndex < 0 || nodeIndex >= (int)gltf.nodes.size()) return;
        const auto& node = gltf.nodes[nodeIndex];

        glm::mat4 local = MatFromNodeTRS(node);
        glm::mat4 cur   = parent * local;

        if (node.mesh >= 0 && node.mesh < (int)gltf.meshes.size()) {
            const auto& mesh = gltf.meshes[node.mesh];

            for (const auto& prim : mesh.primitives) {
                // 读取 POSITION/NORMAL/UV
                std::vector<float> pos, nrm, uv;
                auto itPos = prim.attributes.find("POSITION");
                if (itPos == prim.attributes.end()) continue;

                if (!ReadAccessorFloat(gltf, itPos->second, 3, pos)) continue;

                auto itN = prim.attributes.find("NORMAL");
                if (itN != prim.attributes.end()) ReadAccessorFloat(gltf, itN->second, 3, nrm);

                auto itUV = prim.attributes.find("TEXCOORD_0");
                if (itUV != prim.attributes.end()) ReadAccessorFloat(gltf, itUV->second, 2, uv);

                // 索引
                std::vector<std::uint32_t> idx;
                if (prim.indices >= 0) {
                    ReadIndicesU32(gltf, prim.indices, idx);
                } else {
                    // 无索引则生成顺序索引
                    const size_t vcount = pos.size() / 3;
                    idx.resize(vcount);
                    for (size_t i = 0; i < vcount; ++i) idx[i] = (std::uint32_t)i;
                }

                if (nrm.empty()) {
                    GenerateNormalsIfMissing(pos, idx, nrm);
                }

                // 组装 interleaved 顶点：pos3 + nrm3 + uv2
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

        if (p.albedoTex) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }
}
