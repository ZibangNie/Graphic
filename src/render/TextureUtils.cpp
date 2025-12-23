/*
 * TextureUtils.cpp
 *
 * Purpose:
 *   Implements small texture-related utilities used by the renderer, primarily for sky rendering:
 *     - Load an HDR equirectangular environment map into a 2D floating-point texture
 *     - Convert an HDR equirectangular 2D texture into a cubemap via an offscreen render pass
 *
 * Key techniques:
 *   - HDR loading via stb_image (stbi_loadf) into GL_RGB16F / GL_RGBA16F textures
 *   - Equirectangular-to-cubemap conversion by rendering a unit cube six times into cubemap faces
 *     using a dedicated shader (equirect2cube.vert/.frag)
 *
 * Assumptions / conventions:
 *   - HDR images are treated as linear data; no gamma correction is applied at load time.
 *   - stbi_set_flip_vertically_on_load(false) is used (expected for environment maps).
 *   - Cubemap faces are allocated as GL_RGB16F and rendered at cubeSize x cubeSize.
 *
 * Resource management notes:
 *   - CreateCubeVAO() allocates VAO/VBO but does not delete the VBO (intentionally simplified).
 *     In a stricter resource model, the VBO handle should be stored and released on shutdown.
 */

#include <iostream>
#include <string>
#include <array>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "stb_image.h"
#include "render/Shader.h"

namespace TextureUtils {

/*
 * Creates a VAO for a standard unit cube rendered with position-only vertices.
 *
 * Returns:
 *   VAO handle for a cube with 36 vertices (12 triangles).
 *
 * Vertex layout:
 *   - Attribute location 0: vec3 position
 *
 * Notes:
 *   - The VBO is intentionally not deleted here because the VAO references it.
 *     If full cleanup is required, return/store the VBO handle as well and delete both later.
 */
static GLuint CreateCubeVAO() {
    // Standard unit cube (36 vertices).
    float vertices[] = {
        // positions
        -1,-1,-1,  1,-1,-1,  1, 1,-1,  1, 1,-1, -1, 1,-1, -1,-1,-1,
        -1,-1, 1,  1,-1, 1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1,-1, 1,
        -1, 1, 1, -1, 1,-1, -1,-1,-1, -1,-1,-1, -1,-1, 1, -1, 1, 1,
         1, 1, 1,  1, 1,-1,  1,-1,-1,  1,-1,-1,  1,-1, 1,  1, 1, 1,
        -1,-1,-1,  1,-1,-1,  1,-1, 1,  1,-1, 1, -1,-1, 1, -1,-1,-1,
        -1, 1,-1,  1, 1,-1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1, 1,-1
    };

    GLuint vao=0, vbo=0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    // VBO is intentionally retained since the VAO references it.
    return vao;
}

/*
 * Loads an HDR image file into an OpenGL 2D texture.
 *
 * Parameters:
 *   path : Filesystem path to an HDR image (commonly .hdr).
 *
 * Returns:
 *   OpenGL texture handle (GL_TEXTURE_2D) on success; 0 on failure.
 *
 * Texture format:
 *   - Internal: GL_RGB16F or GL_RGBA16F (depending on channel count)
 *   - Data: GL_FLOAT
 *
 * Sampler state:
 *   - Wrap: GL_CLAMP_TO_EDGE
 *   - Filter: GL_LINEAR (no mipmaps)
 *
 * Notes:
 *   - Uses stbi_loadf, which returns floating-point linear HDR values.
 *   - Vertical flipping is disabled for typical environment map conventions.
 */
GLuint LoadHDRTexture2D(const std::string& path) {
    stbi_set_flip_vertically_on_load(false);

    int w=0, h=0, comp=0;
    float* data = stbi_loadf(path.c_str(), &w, &h, &comp, 0);
    if (!data) {
        std::cerr << "[HDR] Failed to load: " << path << "\n";
        return 0;
    }

    GLenum format = (comp == 3) ? GL_RGB : GL_RGBA;
    GLuint tex=0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    // 16F is generally sufficient and reduces memory vs 32F.
    GLint internal = (format == GL_RGB) ? GL_RGB16F : GL_RGBA16F;

    glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, format, GL_FLOAT, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

/*
 * Converts an HDR equirectangular 2D texture into a cubemap texture.
 *
 * Parameters:
 *   hdrTex2D : OpenGL handle to an equirectangular environment map (GL_TEXTURE_2D, HDR).
 *   cubeSize : Resolution (width/height) for each cubemap face in pixels.
 *   e2cVert  : Vertex shader path for equirect->cubemap conversion.
 *   e2cFrag  : Fragment shader path for equirect->cubemap conversion.
 *
 * Returns:
 *   OpenGL cubemap handle (GL_TEXTURE_CUBE_MAP) on success; 0 on failure.
 *
 * Render strategy:
 *   - Creates an FBO with a depth renderbuffer.
 *   - Allocates a GL_TEXTURE_CUBE_MAP with 6 faces (GL_RGB16F).
 *   - For each face:
 *       - Attaches the face as GL_COLOR_ATTACHMENT0
 *       - Renders a cube with the face-specific view matrix
 *
 * State notes:
 *   - Saves and restores the previous viewport.
 *   - Enables depth testing for cube rendering.
 *
 * Output sampler state:
 *   - Wrap: GL_CLAMP_TO_EDGE on all axes
 *   - Filter: GL_LINEAR
 */
GLuint EquirectHDRToCubemap(GLuint hdrTex2D,
                            int cubeSize,
                            const std::string& e2cVert,
                            const std::string& e2cFrag) {
    Shader shader;
    if (!shader.loadFromFiles(e2cVert, e2cFrag)) {
        std::cerr << "[E2C] Failed to load shader.\n";
        return 0;
    }

    GLuint cubeVAO = CreateCubeVAO();

    GLuint fbo=0, rbo=0;
    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &rbo);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, cubeSize, cubeSize);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

    GLuint cubemap=0;
    glGenTextures(1, &cubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
    for (int i=0;i<6;i++) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
                     GL_RGB16F, cubeSize, cubeSize, 0, GL_RGB, GL_FLOAT, nullptr);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // 90° projection for cube face rendering.
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

    // View matrices for the 6 cubemap faces (right/left/up/down/front/back).
    std::array<glm::mat4, 6> views = {
        glm::lookAt(glm::vec3(0), glm::vec3( 1, 0, 0), glm::vec3(0,-1, 0)),
        glm::lookAt(glm::vec3(0), glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0)),
        glm::lookAt(glm::vec3(0), glm::vec3( 0, 1, 0), glm::vec3(0, 0, 1)),
        glm::lookAt(glm::vec3(0), glm::vec3( 0,-1, 0), glm::vec3(0, 0,-1)),
        glm::lookAt(glm::vec3(0), glm::vec3( 0, 0, 1), glm::vec3(0,-1, 0)),
        glm::lookAt(glm::vec3(0), glm::vec3( 0, 0,-1), glm::vec3(0,-1, 0))
    };

    // Preserve viewport and restore after offscreen conversion.
    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    glViewport(0, 0, cubeSize, cubeSize);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    shader.use();
    shader.setInt("uEquirect", 0);
    shader.setMat4("uProj", proj);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTex2D);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    for (int face=0; face<6; face++) {
        shader.setMat4("uView", views[face]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, cubemap, 0);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

    glDeleteRenderbuffers(1, &rbo);
    glDeleteFramebuffers(1, &fbo);

    // cubeVAO/VBO cleanup is intentionally omitted in this simplified utility.
    // For full cleanup, store VBO handle from CreateCubeVAO() and delete both VAO and VBO here.

    return cubemap;
}

} // namespace TextureUtils
