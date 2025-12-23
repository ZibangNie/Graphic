#include <iostream>
#include <vector>
#include <algorithm>
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

#include "environment/Environment.h"
#include "environment/Sky.h"
#include "environment/Water.h"
#include "environment/Particles.h"


#include "render/LightingSystem.h"
#include "render/Model.h"


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

static GLuint CreateSolidTexture2D(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    unsigned char pixel[4] = { r, g, b, a };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0);
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
    auto rockyPath = (assetsRoot / "textures/rocky/rocky_terrain_02_diff_2k.jpg").string();
    auto sandPath  = (assetsRoot / "textures/sand/sandy_gravel_02_diff_2k.jpg").string();

    GLuint texRocky = LoadTexture2D(rockyPath);
    GLuint texSand  = LoadTexture2D(sandPath);

    // 强制检查：任何一个失败都不允许“悄悄变黑”
    if (texRocky == 0) {
        std::cerr << "[Terrain] Rocky texture failed, using fallback. path=" << rockyPath << "\n";
        texRocky = CreateSolidTexture2D(180, 180, 180); // 灰色
    }
    if (texSand == 0) {
        std::cerr << "[Terrain] Sand texture failed, using fallback. path=" << sandPath << "\n";
        texSand = CreateSolidTexture2D(200, 190, 140); // 沙色
    }


    Shader shader;
    shader.loadFromFiles((assetsRoot / "shaders/basic.vert").string(),
                         (assetsRoot / "shaders/basic.frag").string());

    Shader terrainShader;
    terrainShader.loadFromFiles((assetsRoot / "shaders/terrain.vert").string(),
                                (assetsRoot / "shaders/terrain.frag").string());

    Shader modelShader;
    modelShader.loadFromFiles((assetsRoot / "shaders/model.vert").string(),
                              (assetsRoot / "shaders/model.frag").string());

    Model boat;
    {
        auto boatPath = (assetsRoot / "models/boat.glb").string();
        if (!boat.loadFromGLB(boatPath)) {
            std::cerr << "[Boat] load failed: " << boatPath << "\n";
        }
    }

    // -------------------------
    // Boat tuning (edit here)
    // -------------------------
    static glm::vec3 g_boatPosWS = glm::vec3(-13.0f, 0.0f, -5.0f);  // 世界坐标 x,y,z（你自己调）
    static float     g_boatYawDeg = 90.0f;                      // 绕Y旋转角度（你自己调）
    static float     g_boatScale  = 3.0f;                       // 缩放（你自己调）
    static float     g_boatYOffset = 0.05f;                     // 额外抬高，防止z-fighting（你自己调）
    static bool      g_boatUseWaterHeight = true;               // true: y=水面高度; false: 使用 g_boatPosWS.y

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

    Environment environment;

    Sky sky;
    if (!sky.init(assetsRoot,
              "textures/sky/syferfontein_0d_clear_puresky_4k.hdr",
              "textures/sky/qwantani_night_puresky_4k.hdr",
              512)) {
        std::cerr << "[Main] Sky init failed.\n";
              }

    LightingSystem lighting;

    Water water;
    if (!water.init(assetsRoot, g_fbW, g_fbH,
                    tc.waterHeight,
                    terrain.MinX(), terrain.MaxX(),
                    terrain.MinZ(), terrain.MaxZ())) {
        std::cerr << "[Main] Water init failed.\n";
                    }

    int lastFbW = g_fbW;
    int lastFbH = g_fbH;

    // Campfire particle system (flame + embers + glow)
    ParticleSystem fire;
    if (!fire.init((assetsRoot / "shaders/particle.vert").string(),
                   (assetsRoot / "shaders/particle.frag").string(),
                   2500)) {
        std::cerr << "[Main] ParticleSystem init failed.\n";
                   }


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

        // 1) 更新环境
        environment.update(dt);

        // Update campfire emitter near the player (small, offset to the side)
        glm::vec3 firePos = player.position() + glm::vec3(1.20f, 0.0f, 0.80f);
        firePos.x = std::max(terrain.MinX() + 0.6f, std::min(firePos.x, terrain.MaxX() - 0.6f));
        firePos.z = std::max(terrain.MinZ() + 0.6f, std::min(firePos.z, terrain.MaxZ() - 0.6f));
        firePos.y = terrain.GetHeight(firePos.x, firePos.z) + 0.02f;

        fire.setCampfirePosition(firePos);
        fire.update(dt, static_cast<float>(now));


        // 如果窗口尺寸变了，重建反射 FBO
        if (g_fbW != lastFbW || g_fbH != lastFbH) {
            water.resize(g_fbW, g_fbH);
            lastFbW = g_fbW;
            lastFbH = g_fbH;
        }

        // -------------------------
        // Pass A: Reflection FBO
        // -------------------------
        // 构造镜像相机（关于 y = waterHeight 镜像）
        Camera camRef = camera;
        camRef.position.y = 2.0f * tc.waterHeight - camera.position.y;
        camRef.pivot.y    = 2.0f * tc.waterHeight - camera.pivot.y;
        glm::mat4 viewRef = camRef.getViewMatrix();

        // clip plane：只保留水面以上（dot >= 0）
        // y - waterY + eps >= 0 => y >= waterY - eps
        const float clipEps = 0.02f;
        glm::vec4 clipPlaneAbove(0.0f, 1.0f, 0.0f, -tc.waterHeight + clipEps);

        // 给会参与反射绘制的 shader 设 clip plane
        terrainShader.use();
        terrainShader.setVec4("uClipPlane", clipPlaneAbove);

        shader.use();
        shader.setVec4("uClipPlane", clipPlaneAbove);

        glEnable(GL_CLIP_DISTANCE0);

        water.beginReflectionPass();

        // 反射 pass 中也画天空盒（用镜像相机）
        sky.render(camRef, proj, environment);

        // 反射 pass：同样送光照（cameraPos 用镜像相机更严谨）
        lighting.applyFromEnvironment(terrainShader, camRef, environment);
        lighting.applyFromEnvironment(shader, camRef, environment);

        // 画场景（不画水）
        world.drawRecursive(viewRef, proj);

        // --- Draw boat (reflection pass) ---
        {
            modelShader.use();
            modelShader.setVec4("uClipPlane", clipPlaneAbove);

            // 固定位置（世界坐标）
            glm::vec3 boatPos = g_boatPosWS;
            if (g_boatUseWaterHeight) {
                boatPos.y = tc.waterHeight + g_boatYOffset;
            } else {
                boatPos.y = boatPos.y + g_boatYOffset;
            }

            glm::mat4 M(1.0f);
            M = glm::translate(M, boatPos);
            M = glm::rotate(M, glm::radians(g_boatYawDeg), glm::vec3(0,1,0));
            M = glm::scale(M, glm::vec3(g_boatScale));

            // 只对船局部禁用剔除，避免 glTF 绕序问题，同时不污染天空/地形状态
            GLboolean wasCull = glIsEnabled(GL_CULL_FACE);
            glDisable(GL_CULL_FACE);

            boat.draw(modelShader, M, viewRef, proj);

            if (wasCull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        }

        fire.render(camRef, viewRef, proj, clipPlaneAbove);


        water.endReflectionPass(g_fbW, g_fbH);

        glDisable(GL_CLIP_DISTANCE0);

        // 还原 clip plane（永远不过裁剪）
        glm::vec4 clipPlaneOff(0.0f, 1.0f, 0.0f, 1000000.0f);
        terrainShader.use();
        terrainShader.setVec4("uClipPlane", clipPlaneOff);
        shader.use();
        shader.setVec4("uClipPlane", clipPlaneOff);

        // -------------------------
        // Pass B: Normal scene
        // -------------------------
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        sky.render(camera, proj, environment);

        lighting.applyFromEnvironment(terrainShader, camera, environment);
        lighting.applyFromEnvironment(shader, camera, environment);
        world.drawRecursive(view, proj);

        // --- Draw boat (main pass) ---
        {
            modelShader.use();
            modelShader.setVec4("uClipPlane", clipPlaneOff);

            glm::vec3 boatPos = g_boatPosWS;
            if (g_boatUseWaterHeight) {
                boatPos.y = tc.waterHeight + g_boatYOffset;
            } else {
                boatPos.y = boatPos.y + g_boatYOffset;
            }

            glm::mat4 M(1.0f);
            M = glm::translate(M, boatPos);
            M = glm::rotate(M, glm::radians(g_boatYawDeg), glm::vec3(0,1,0));
            M = glm::scale(M, glm::vec3(g_boatScale));

            GLboolean wasCull = glIsEnabled(GL_CULL_FACE);
            glDisable(GL_CULL_FACE);

            boat.draw(modelShader, M, view, proj);

            if (wasCull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        }



        //fire.render(camRef, viewRef, proj, clipPlaneAbove);

        // -------------------------
        // Pass C: Water surface
        // -------------------------
        water.render(camera, view, proj, viewRef, environment, lighting, (float)now);

        // Campfire particles (draw after water; additive blend + no depth write)
        fire.render(camera, view, proj, clipPlaneOff);

        glm::vec3 worldPivot(0.0f, 0.0f, 0.0f);
        glm::vec3 sunDir = glm::normalize(environment.sun().light().direction);

        if (sunDir.y > 0.0f) {
            glm::vec3 sunPos = worldPivot + sunDir * 120.0f;

            shader.use();
            shader.setInt("uEmissive", 1);
            shader.setVec3("uTint", glm::vec3(1.0f, 0.9f, 0.6f));
            shader.setMat4("uView", view);
            shader.setMat4("uProj", proj);

            glm::mat4 sunModel(1.0f);
            sunModel = glm::translate(sunModel, sunPos);
            sunModel = glm::scale(sunModel, glm::vec3(1.5f));
            shader.setMat4("uModel", sunModel);

            boxMesh.draw();
            shader.setInt("uEmissive", 0);
        }


        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
