#include <iostream>
#include <vector>
#include <filesystem>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "stb_image.h"

#include "render/Shader.h"
#include "render/Mesh.h"

#include "core/Input.h"
#include "scene/Camera.h"
#include "scene/SceneNode.h"
#include "scene/Player.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "scene/Terrain.h"
#include "render/LightingSystem.h"

static int g_fbW = 1280;
static int g_fbH = 720;

static void FramebufferSizeCallback(GLFWwindow*, int w, int h) {
    g_fbW = (w > 0) ? w : 1;
    g_fbH = (h > 0) ? h : 1;
    glViewport(0, 0, g_fbW, g_fbH);
}

static std::filesystem::path FindAssetsRoot() {
    namespace fs = std::filesystem;
    fs::path p = fs::current_path();
    for (int i = 0; i < 8; ++i) {
        fs::path cand = p / "assets";
        if (fs::exists(cand) && fs::is_directory(cand)) return cand;
        if (!p.has_parent_path()) break;
        p = p.parent_path();
    }
    return {};
}

static GLuint LoadTexture2D(const std::string& path) {
    int w=0, h=0, comp=0;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << "\n";
        return 0;
    }

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

// Build a unit cube centered at origin using triangles, with per-vertex color = (1,1,1)
// Vertex format: pos(3) + color(3)
static Mesh CreateUnitCubeMesh() {
    Mesh m;

    auto push = [](std::vector<float>& v, float x, float y, float z) {
        v.push_back(x); v.push_back(y); v.push_back(z);
        v.push_back(1.0f); v.push_back(1.0f); v.push_back(1.0f);
    };

    // cube from -0.5..+0.5
    const float a = -0.5f, b = 0.5f;

    // 36 vertices (12 triangles)
    std::vector<float> v;
    v.reserve(36 * 6);

    // +Z (front)
    push(v, a, a, b); push(v, b, a, b); push(v, b, b, b);
    push(v, a, a, b); push(v, b, b, b); push(v, a, b, b);

    // -Z (back)
    push(v, b, a, a); push(v, a, a, a); push(v, a, b, a);
    push(v, b, a, a); push(v, a, b, a); push(v, b, b, a);

    // +X (right)
    push(v, b, a, b); push(v, b, a, a); push(v, b, b, a);
    push(v, b, a, b); push(v, b, b, a); push(v, b, b, b);

    // -X (left)
    push(v, a, a, a); push(v, a, a, b); push(v, a, b, b);
    push(v, a, a, a); push(v, a, b, b); push(v, a, b, a);

    // +Y (top)
    push(v, a, b, b); push(v, b, b, b); push(v, b, b, a);
    push(v, a, b, b); push(v, b, b, a); push(v, a, b, a);

    // -Y (bottom)
    push(v, a, a, a); push(v, b, a, a); push(v, b, a, b);
    push(v, a, a, a); push(v, b, a, b); push(v, a, a, b);

    m.uploadInterleavedPosColor(v);
    return m;
}

int main() {
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Steve - Hierarchy Transform", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD\n";
        return -1;
    }

    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    glfwGetFramebufferSize(window, &g_fbW, &g_fbH);
    glViewport(0, 0, g_fbW, g_fbH);

    glEnable(GL_DEPTH_TEST);

    // assets
    auto assetsRoot = FindAssetsRoot();
    if (assetsRoot.empty()) {
        std::cerr << "Failed to locate assets directory. CWD=" << std::filesystem::current_path() << "\n";
        return -1;
    }

    // 这里路径按你实际 assets 目录改：你说 assets 外面有 rocky 和 sand 两个材质
    // 例如：assets/textures/rocky/xxx_diff.jpg, assets/textures/sand/xxx_diff.jpg
    GLuint texRocky = LoadTexture2D((assetsRoot / "textures/rocky/rocky_terrain_02_diff_2k.jpg").string());
    GLuint texSand  = LoadTexture2D((assetsRoot / "textures/sand/sandy_gravel_02_diff_2k.jpg").string());

    Shader shader;
    shader.loadFromFiles((assetsRoot / "shaders/basic.vert").string(),
                         (assetsRoot / "shaders/basic.frag").string());

    Shader terrainShader;
    terrainShader.loadFromFiles((assetsRoot / "shaders/terrain.vert").string(),
                                (assetsRoot / "shaders/terrain.frag").string());

    // Input + Camera
    Input input(window);
    Camera camera;
    camera.position = {0.f, 2.0f, 6.0f};

    // Scene root
    SceneNode world("WorldRoot");

    // Box mesh
    Mesh boxMesh = CreateUnitCubeMesh();

    // Player (Steve)
    Player player;
    player.build(world, boxMesh, shader);

    // ------------------------
    // Terrain tuning block
    // ------------------------
    struct TerrainConfig {
        // Geometry
        int   widthVerts   = 320;   // vertex count in X
        int   depthVerts   = 320;   // vertex count in Z
        float gridSpacing  = 0.50f; // world units between vertices (bigger -> larger terrain, same tri count)

        // Shape
        float noiseScale   = 0.08f; // smaller -> broader hills; larger -> noisier detail
        float heightScale  = 10.0f;  // vertical amplitude
        int   seed         = 1337;  // change to get a different map

        // Water (reserved for later)
        float waterHeight  = -0.5f; // used by color ramp now; later used by water plane rendering
    };

    TerrainConfig tc;


    Terrain terrain(tc.widthVerts, tc.depthVerts, tc.gridSpacing);
    terrain.waterHeight = tc.waterHeight;
    terrain.Build(tc.noiseScale, tc.heightScale, tc.seed);

    auto terrainNode = std::make_unique<SceneNode>("Terrain");
    terrainNode->mesh = &terrain.mesh();
    terrainNode->shader = &terrainShader;

    terrainNode->tex0 = texRocky;
    terrainNode->tex1 = texSand;

    // 纹理密度：越大 = 纹理重复越多。建议从 0.05 起调。
    // 经验：地图越大、gridSpacing 越大，uvScale 通常要更小一点才不“密得像噪声”。
    terrainNode->uvScale = 0.05f;

    // 低处为 sand：直接用你预留的水位线 tc.waterHeight
    terrainNode->sandHeight = tc.waterHeight;

    // 过渡带宽度（世界单位）：越大过渡越柔和
    terrainNode->blendWidth = 0.35f;

    // tint 保持白色，不影响贴图
    terrainNode->tint = {1.0f, 1.0f, 1.0f};

    // IMPORTANT: keep tint = white so vertex colors are not altered
    terrainNode->tint = {1.0f, 1.0f, 1.0f};

    terrainNode->transform.setLocalScale({1.0f, 1.0f, 1.0f});
    terrainNode->transform.setLocalPosition({0.0f, 0.0f, 0.0f});
    world.addChild(std::move(terrainNode));


    double lastTime = glfwGetTime();
    LightingSystem lighting;


    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = static_cast<float>(now - lastTime);
        lastTime = now;

        glfwPollEvents();
        input.update();

        if (input.keyDown(GLFW_KEY_ESCAPE)) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        camera.updateOrbit(input, player.position());      // consume RMB/scroll first
        player.update(input, dt, terrain, camera);         // move relative to current camera
        camera.updateOrbitNoInput(player.position());      // re-center to new player pos
        glm::mat4 view = camera.getViewMatrix();


        float aspect = static_cast<float>(g_fbW) / static_cast<float>(g_fbH);

        glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 200.0f);

        lighting.update(dt, input);
        lighting.applyTo(terrainShader, camera);
        lighting.applyTo(shader, camera);



        glClearColor(0.08f, 0.10f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        world.drawRecursive(view, proj);

        // 在 world.drawRecursive(view, proj); 之后
        // 选一个固定的世界参考点：地形中心（更合理）或原点
        glm::vec3 worldPivot(0.0f, 0.0f, 0.0f);
        glm::vec3 sunPos = worldPivot - lighting.state().sunDir * 120.0f;

        shader.use();
        shader.setInt("uEmissive", 1);
        shader.setVec3("uTint", glm::vec3(1.0f, 0.9f, 0.6f));
        shader.setMat4("uView", view);
        shader.setMat4("uProj", proj);
        shader.setVec3("uTint", glm::vec3(1.0f, 0.9f, 0.6f)); // 太阳颜色

        glm::mat4 sunModel(1.0f);
        sunModel = glm::translate(sunModel, sunPos);
        sunModel = glm::scale(sunModel, glm::vec3(1.5f));     // 太阳大小
        shader.setMat4("uModel", sunModel);

        boxMesh.draw();
        shader.setInt("uEmissive", 0); // 画完太阳后恢复


        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
