/*
 * main.cpp
 *
 * Purpose:
 *   Application entry point and frame loop for the assignment scene.
 *   Responsibilities include:
 *     - GLFW / GLAD initialization and window lifecycle
 *     - Asset root discovery and texture/shader loading
 *     - Scene construction (player, terrain, sky, water, particles, model)
 *     - Per-frame update (input, camera orbit, player movement, environment time)
 *     - Multi-pass rendering:
 *         Pass A: reflection render into water FBO (with clip plane)
 *         Pass B: main scene render
 *         Pass C: water surface compositing using reflection texture
 *         + additive particle rendering and emissive sun disk
 *
 * Render conventions:
 *   - Coordinate system: world Y is up.
 *   - Clip plane: dot(vec4(worldPos,1), plane) < 0 => clipped (discard / clip distance).
 *   - Skybox uses view matrix without translation (rotational component only).
 *
 * Performance notes:
 *   - Water reflection FBO uses half-resolution to reduce fill cost.
 *   - Terrain uses a dense grid; width/depth and spacing should be tuned to avoid excessive triangles.
 */

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

/*
 * GLFW framebuffer resize callback.
 *
 * Parameters:
 *   w, h : New framebuffer dimensions in pixels. Values may be 0 on minimize.
 *
 * Notes:
 *   - Updates the global framebuffer size used for projection and FBO sizing.
 *   - Immediately updates GL viewport for subsequent rendering.
 */
static void FramebufferSizeCallback(GLFWwindow*, int w, int h) {
    g_fbW = (w > 0) ? w : 1;
    g_fbH = (h > 0) ? h : 1;
    glViewport(0, 0, g_fbW, g_fbH);
}

/*
 * Attempts to locate the project assets directory by walking up from CWD.
 *
 * Returns:
 *   Path to ".../assets" if found; empty path otherwise.
 *
 * Notes:
 *   - Search depth is limited to avoid unbounded traversal.
 *   - Expected project layout: <project_root>/assets/...
 */
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

/*
 * Loads an 8-bit texture from disk into an OpenGL 2D texture.
 *
 * Parameters:
 *   path : Filesystem path to PNG/JPG/etc.
 *
 * Returns:
 *   OpenGL texture ID (0 on failure).
 *
 * Notes:
 *   - Enables vertical flip for typical image coordinate conventions (top-left origin).
 *   - Mipmaps are generated; min filter uses trilinear sampling.
 *   - Wrap mode is REPEAT (appropriate for tiling terrain textures).
 */
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

/*
 * Creates a 1x1 solid-color RGBA texture.
 *
 * Parameters:
 *   r,g,b,a : 8-bit channel values.
 *
 * Returns:
 *   OpenGL texture ID.
 *
 * Notes:
 *   - Intended as a safe fallback when disk texture loading fails.
 *   - Uses NEAREST to avoid filtering artifacts on constant textures.
 */
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

/*
 * Builds a unit cube mesh centered at the origin, using non-indexed triangles.
 *
 * Returns:
 *   Mesh with vertex format: position(3) + color(3).
 *
 * Notes:
 *   - Per-vertex color is initialized to white; final tinting is handled in shaders (uTint).
 *   - Used as a generic primitive for the player blocks and the emissive sun indicator.
 */
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

/*
 * Program entry point.
 *
 * High-level flow:
 *   1) Initialize GLFW + create OpenGL context
 *   2) Load GL functions via GLAD
 *   3) Locate assets and load textures/shaders/models
 *   4) Construct scene graph and simulation systems
 *   5) Run main loop:
 *        - input + camera + player update
 *        - environment update (time-of-day / sun)
 *        - reflection pass (water FBO + clip plane)
 *        - main pass (scene + model)
 *        - water surface pass (sample reflection)
 *        - additive particles and emissive sun marker
 *   6) Cleanup and exit
 */
int main() {
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Steve - Hierarchy Transform", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // VSync on

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

    // Terrain textures.
    // Note: paths assume the project asset layout includes:
    //   assets/textures/rocky/<file>
    //   assets/textures/sand/<file>
    auto rockyPath = (assetsRoot / "textures/rocky/rocky_terrain_02_diff_2k.jpg").string();
    auto sandPath  = (assetsRoot / "textures/sand/sandy_gravel_02_diff_2k.jpg").string();

    GLuint texRocky = LoadTexture2D(rockyPath);
    GLuint texSand  = LoadTexture2D(sandPath);

    // Defensive fallback: never allow missing terrain textures to silently black out.
    if (texRocky == 0) {
        std::cerr << "[Terrain] Rocky texture failed, using fallback. path=" << rockyPath << "\n";
        texRocky = CreateSolidTexture2D(180, 180, 180); // neutral gray
    }
    if (texSand == 0) {
        std::cerr << "[Terrain] Sand texture failed, using fallback. path=" << sandPath << "\n";
        texSand = CreateSolidTexture2D(200, 190, 140); // sand-like color
    }

    // Basic untextured shader (used for block-style geometry / emissive sun marker).
    Shader shader;
    shader.loadFromFiles((assetsRoot / "shaders/basic.vert").string(),
                         (assetsRoot / "shaders/basic.frag").string());

    // Terrain shader (texture blending + lighting).
    Shader terrainShader;
    terrainShader.loadFromFiles((assetsRoot / "shaders/terrain.vert").string(),
                                (assetsRoot / "shaders/terrain.frag").string());

    // Model shader (glTF baseColor texture + factor).
    Shader modelShader;
    modelShader.loadFromFiles((assetsRoot / "shaders/model.vert").string(),
                              (assetsRoot / "shaders/model.frag").string());

    // glTF model: boat.
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
    // Notes:
    //   - g_boatPosWS: world-space anchor position. If g_boatUseWaterHeight is true,
    //     Y is overridden to water height (+ optional offset).
    //   - g_boatYawDeg: yaw rotation around world Y axis (degrees).
    //   - g_boatScale: uniform scale multiplier for the imported model.
    //   - g_boatYOffset: small positive offset to avoid z-fighting with water surface.
    static glm::vec3 g_boatPosWS = glm::vec3(-13.0f, 0.0f, -5.0f);  // world x,y,z
    static float     g_boatYawDeg = 90.0f;                         // degrees
    static float     g_boatScale  = 3.0f;                          // >0
    static float     g_boatYOffset = 0.05f;                        // small lift
    static bool      g_boatUseWaterHeight = true;                  // true: y=water height

    // Input + Camera
    Input input(window);
    Camera camera;
    camera.position = {0.f, 2.0f, 6.0f};

    // Scene root
    SceneNode world("WorldRoot");

    // Box mesh primitive
    Mesh boxMesh = CreateUnitCubeMesh();

    // Player (Steve)
    Player player;
    player.build(world, boxMesh, shader);

    // ------------------------
    // Terrain tuning block
    // ------------------------
    struct TerrainConfig {
        // Geometry
        int   widthVerts   = 320;   // vertex count in X (>= 2)
        int   depthVerts   = 320;   // vertex count in Z (>= 2)
        float gridSpacing  = 0.50f; // world units between vertices

        // Shape
        float noiseScale   = 0.08f; // smaller -> broader hills; larger -> higher-frequency detail
        float heightScale  = 10.0f; // vertical amplitude (world units)
        int   seed         = 1337;  // deterministic map seed

        // Water (shared reference height used by terrain and water system)
        float waterHeight  = -0.5f;
    };

    TerrainConfig tc;

    Terrain terrain(tc.widthVerts, tc.depthVerts, tc.gridSpacing);
    terrain.waterHeight = tc.waterHeight;
    terrain.Build(tc.noiseScale, tc.heightScale, tc.seed);

    // Terrain scene node: binds textures + material parameters for terrain shader.
    auto terrainNode = std::make_unique<SceneNode>("Terrain");
    terrainNode->mesh = &terrain.mesh();
    terrainNode->shader = &terrainShader;

    terrainNode->tex0 = texRocky;
    terrainNode->tex1 = texSand;

    // UV scaling (world-space tiling frequency).
    // Typical range: ~0.02 .. 0.15 depending on terrain size and desired detail density.
    terrainNode->uvScale = 0.05f;

    // Height threshold where sand begins (usually aligned with water height).
    terrainNode->sandHeight = tc.waterHeight;

    // Smooth blend region width (world units). Typical range: ~0.15 .. 1.0.
    terrainNode->blendWidth = 0.35f;

    // Keep tint white so terrain albedo is not unintentionally colored.
    terrainNode->tint = {1.0f, 1.0f, 1.0f};

    // Transform is identity (terrain already authored in world coordinates).
    terrainNode->transform.setLocalScale({1.0f, 1.0f, 1.0f});
    terrainNode->transform.setLocalPosition({0.0f, 0.0f, 0.0f});
    world.addChild(std::move(terrainNode));

    double lastTime = glfwGetTime();

    // Environment state (time-of-day + sun directional light).
    Environment environment;

    // Sky system (HDR equirect -> cubemap; day/night blend).
    Sky sky;
    if (!sky.init(assetsRoot,
              "textures/sky/syferfontein_0d_clear_puresky_4k.hdr",
              "textures/sky/qwantani_night_puresky_4k.hdr",
              512)) {
        std::cerr << "[Main] Sky init failed.\n";
              }

    // Lighting bridge: writes environment lighting uniforms into shaders.
    LightingSystem lighting;

    // Water system: owns reflection FBO and draws water surface sampling the reflection texture.
    Water water;
    if (!water.init(assetsRoot, g_fbW, g_fbH,
                    tc.waterHeight,
                    terrain.MinX(), terrain.MaxX(),
                    terrain.MinZ(), terrain.MaxZ())) {
        std::cerr << "[Main] Water init failed.\n";
                    }

    int lastFbW = g_fbW;
    int lastFbH = g_fbH;

    // Campfire particle system (flame + embers + glow).
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

        // Global quit.
        if (input.keyDown(GLFW_KEY_ESCAPE)) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        // Camera orbit consumes RMB + scroll input; player movement uses keyboard axes.
        camera.updateOrbit(input, player.position());      // consume RMB/scroll first
        player.update(input, dt, terrain, camera);         // move relative to current camera
        camera.updateOrbitNoInput(player.position());      // re-center to new player pos
        glm::mat4 view = camera.getViewMatrix();

        float aspect = static_cast<float>(g_fbW) / static_cast<float>(g_fbH);

        // Perspective parameters:
        //   - FOV: 60 deg is a typical third-person baseline.
        //   - Near: 0.1 (avoid clipping nearby geometry).
        //   - Far: 200 (must exceed terrain extents + skybox depth usage).
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 200.0f);

        // 1) Update environment (time-of-day + sun direction/intensity).
        environment.update(dt);

        // Campfire anchor: offset from player, snapped to terrain height and clamped to bounds.
        glm::vec3 firePos = player.position() + glm::vec3(1.20f, 0.0f, 0.80f);
        firePos.x = std::max(terrain.MinX() + 0.6f, std::min(firePos.x, terrain.MaxX() - 0.6f));
        firePos.z = std::max(terrain.MinZ() + 0.6f, std::min(firePos.z, terrain.MaxZ() - 0.6f));
        firePos.y = terrain.GetHeight(firePos.x, firePos.z) + 0.02f;

        fire.setCampfirePosition(firePos);
        fire.update(dt, static_cast<float>(now));

        // Resize reflection target when the window framebuffer changes.
        if (g_fbW != lastFbW || g_fbH != lastFbH) {
            water.resize(g_fbW, g_fbH);
            lastFbW = g_fbW;
            lastFbH = g_fbH;
        }

        // -------------------------
        // Pass A: Reflection FBO
        // -------------------------
        // Reflection camera is mirrored about the water plane y = waterHeight.
        Camera camRef = camera;
        camRef.position.y = 2.0f * tc.waterHeight - camera.position.y;
        camRef.pivot.y    = 2.0f * tc.waterHeight - camera.pivot.y;
        glm::mat4 viewRef = camRef.getViewMatrix();

        // Clip plane for reflection rendering:
        // Keep only geometry above the water plane (small epsilon reduces artifacts at the boundary).
        const float clipEps = 0.02f;
        glm::vec4 clipPlaneAbove(0.0f, 1.0f, 0.0f, -tc.waterHeight + clipEps);

        // Apply clip plane to shaders that use either gl_ClipDistance or manual discard.
        terrainShader.use();
        terrainShader.setVec4("uClipPlane", clipPlaneAbove);

        shader.use();
        shader.setVec4("uClipPlane", clipPlaneAbove);

        glEnable(GL_CLIP_DISTANCE0);

        water.beginReflectionPass();

        // Skybox in reflection pass should also use the mirrored camera.
        sky.render(camRef, proj, environment);

        // Lighting uniforms during reflection should be consistent with the mirrored camera position.
        lighting.applyFromEnvironment(terrainShader, camRef, environment);
        lighting.applyFromEnvironment(shader, camRef, environment);

        // Render scene graph (excluding water).
        world.drawRecursive(viewRef, proj);

        // --- Draw boat (reflection pass) ---
        {
            modelShader.use();
            modelShader.setVec4("uClipPlane", clipPlaneAbove);

            // Boat placement:
            //   - If using water height, boat rests on the water plane (+ offset).
            //   - Otherwise uses g_boatPosWS.y (+ offset).
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

            // Local cull override:
            //   - Some glTF assets may have inconsistent winding.
            //   - State is restored to avoid affecting other draws.
            GLboolean wasCull = glIsEnabled(GL_CULL_FACE);
            glDisable(GL_CULL_FACE);

            boat.draw(modelShader, M, viewRef, proj);

            if (wasCull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        }

        // Particles in reflection pass:
        //   - Uses additive blending; clip plane ensures no underwater contribution.
        fire.render(camRef, viewRef, proj, clipPlaneAbove);

        water.endReflectionPass(g_fbW, g_fbH);

        glDisable(GL_CLIP_DISTANCE0);

        // Disable clipping for subsequent passes by setting a plane far away.
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

        // -------------------------
        // Pass C: Water surface
        // -------------------------
        // Water shader samples reflection texture and applies lighting + Fresnel.
        water.render(camera, view, proj, viewRef, environment, lighting, (float)now);

        // Campfire particles:
        //   - Rendered after water so additive blend overlays correctly.
        //   - Particle renderer disables depth writes internally to preserve depth testing.
        fire.render(camera, view, proj, clipPlaneOff);

        // Emissive sun marker (debug/visual indicator).
        // Notes:
        //   - Only drawn when sun is above the horizon (sunDir.y > 0).
        //   - Uses uEmissive path in basic.frag to bypass lighting.
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
