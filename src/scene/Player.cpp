/*
 * Player.cpp
 *
 * Purpose:
 *   Implements a simple Minecraft-style player character:
 *     - Procedural construction of a blocky "Steve-like" hierarchy using SceneNode
 *     - WASD movement relative to the orbit camera on the XZ plane
 *     - Basic walking animation (arm/leg swing)
 *     - Terrain-constrained locomotion (clamp to bounds + stick to heightfield)
 *     - Head tracking toward camera direction (yaw damped; pitch scaled and clamped)
 *
 * Scene graph structure (high level):
 *   PlayerRoot
 *     TorsoPivot (translation only; NOT scaled)
 *       TorsoMesh
 *       HeadJoint
 *         Head mesh, hair shell, face plates (eyes/pupils/mouth)
 *       LeftArmJoint -> LeftArm mesh
 *       RightArmJoint -> RightArm mesh
 *       LeftLegJoint -> LeftLeg mesh (+ shoe)
 *       RightLegJoint -> RightLeg mesh (+ shoe)
 *
 * Coordinate / angle conventions:
 *   - World up is +Y.
 *   - Yaw angles are in degrees and derived via atan2(x, z) on the ground plane.
 *   - Body yaw (m_yawDeg) rotates PlayerRoot around +Y.
 *   - Head yaw is relative to body yaw, clamped to +/- m_headMaxYawDeg.
 *
 * Rendering conventions:
 *   - Each node uses a unit cube Mesh with per-node scale and tint.
 */

#include "scene/Player.h"
#include "scene/SceneNode.h"
#include "core/Input.h"
#include "scene/Terrain.h"
#include "scene/Camera.h"

#include <GLFW/glfw3.h>          // Include after project headers (avoids glad conflicts).
#include <glm/gtc/quaternion.hpp>
#include <cmath>

/*
 * Helper: rotation quaternion around +X by degrees.
 *
 * Parameters:
 *   deg : Angle in degrees.
 */
static glm::quat RotXDeg(float deg) {
    return glm::angleAxis(glm::radians(deg), glm::vec3(1,0,0));
}

/*
 * Helper: rotation quaternion around +Y by degrees.
 *
 * Parameters:
 *   deg : Angle in degrees (yaw).
 */
static glm::quat RotYDeg(float deg) {
    return glm::angleAxis(glm::radians(deg), glm::vec3(0,1,0));
}

/*
 * Builds the player scene-graph hierarchy and attaches it to worldRoot.
 *
 * Parameters:
 *   worldRoot : Root SceneNode for the world; the player root is attached as a child.
 *   boxMesh   : Shared unit-cube mesh used for all body parts (scaled per node).
 *   shader    : Shader used by the player parts; per-node tint is set via SceneNode::tint.
 *
 * Returns:
 *   Pointer to the created player root node within the world scene graph.
 *
 * Notes:
 *   - Uses Minecraft-like proportions (head 8x8x8, torso 8x12x4, limbs 4x12x4) in "pixels".
 *   - unit controls the overall character size in world units.
 *   - TorsoPivot must not be scaled; scaling pivot nodes will unintentionally scale children offsets.
 */
SceneNode* Player::build(SceneNode& worldRoot, Mesh& boxMesh, Shader& shader) {
    // ----- scale unit (Minecraft "pixel") -----
    // Minecraft proportions: head 8, body 12, limbs 12 (in pixels)
    const float unit = 0.10f; // Overall size scale in world units per "pixel".

    const float headW = 8 * unit, headH = 8 * unit, headD = 8 * unit;
    const float bodyW = 8 * unit, bodyH = 12 * unit, bodyD = 4 * unit;
    const float limbW = 4 * unit, limbH = 12 * unit, limbD = 4 * unit;
    const float legW  = 4 * unit, legH  = 12 * unit, legD  = 4 * unit;

    // ----- colors close to Steve -----
    // These are flat albedo tints; lighting/shading is handled by the active shader.
    const glm::vec3 skin  = {0.93f, 0.80f, 0.66f};
    const glm::vec3 hair  = {0.20f, 0.13f, 0.06f};
    const glm::vec3 shirt = {0.20f, 0.55f, 0.90f};
    const glm::vec3 pants = {0.20f, 0.20f, 0.55f};
    const glm::vec3 shoe  = {0.10f, 0.10f, 0.12f};
    const glm::vec3 eyeW  = {0.95f, 0.95f, 0.95f};
    const glm::vec3 eyeB  = {0.10f, 0.20f, 0.60f};
    const glm::vec3 mouth = {0.35f, 0.20f, 0.18f};

    // Root node (moves as player)
    auto playerRoot = std::make_unique<SceneNode>("PlayerRoot");
    playerRoot->transform.setLocalPosition(m_position);

    // ---- TorsoPivot: IMPORTANT (do not scale this node) ----
    auto torsoPivot = std::make_unique<SceneNode>("TorsoPivot");
    // Place torso so that feet are on ground (y=0).
    // Legs length = legH, torso height = bodyH. Torso center at legH + bodyH/2.
    torsoPivot->transform.setLocalPosition({0.0f, legH + bodyH * 0.5f, 0.0f});
    SceneNode* torsoPivotPtr = playerRoot->addChild(std::move(torsoPivot));

    // Torso mesh (scaled unit cube)
    auto torsoMesh = std::make_unique<SceneNode>("TorsoMesh");
    torsoMesh->mesh = &boxMesh;
    torsoMesh->shader = &shader;
    torsoMesh->tint = shirt;
    torsoMesh->transform.setLocalScale({bodyW, bodyH, bodyD});
    torsoMesh->transform.setLocalPosition({0.0f, 0.0f, 0.0f});
    torsoPivotPtr->addChild(std::move(torsoMesh));

    // ---- Head joint at top of torso ----
    auto headJoint = std::make_unique<SceneNode>("HeadJoint");
    // Neck anchor point relative to torsoPivot.
    headJoint->transform.setLocalPosition({0.0f, bodyH * 0.5f, 0.0f});
    SceneNode* headJointPtr = torsoPivotPtr->addChild(std::move(headJoint));
    m_headJoint = headJointPtr;

    // Head mesh centered above neck
    auto headMesh = std::make_unique<SceneNode>("Head");
    headMesh->mesh = &boxMesh;
    headMesh->shader = &shader;
    headMesh->tint = skin;
    headMesh->transform.setLocalScale({headW, headH, headD});
    headMesh->transform.setLocalPosition({0.0f, headH * 0.5f, 0.0f});
    SceneNode* headPtr = headJointPtr->addChild(std::move(headMesh));

    // Hair layer (slightly larger shell)
    auto hairLayer = std::make_unique<SceneNode>("HairLayer");
    hairLayer->mesh = &boxMesh;
    hairLayer->shader = &shader;
    hairLayer->tint = hair;
    // Slight scaling prevents Z-fighting with the head surface.
    hairLayer->transform.setLocalScale({headW * 1.04f, headH * 1.04f, headD * 1.04f});
    hairLayer->transform.setLocalPosition({0.0f, headH * 0.5f, 0.0f});
    headJointPtr->addChild(std::move(hairLayer));

    // Face details: thin quads (implemented as very thin boxes) on the front of head.
    // faceZ pushes plates just in front of head's front face to avoid Z-fighting.
    const float faceZ = headD * 0.5f + unit * 0.50f;
    const float plateT = unit * 0.04f;               // Plate thickness (world units).

    auto makePlate = [&](const char* name, glm::vec3 tint, glm::vec3 scale, glm::vec3 pos) {
        auto n = std::make_unique<SceneNode>(name);
        n->mesh = &boxMesh;
        n->shader = &shader;
        n->tint = tint;
        n->transform.setLocalScale(scale);
        n->transform.setLocalPosition(pos);
        headJointPtr->addChild(std::move(n));
    };

    // Eyes: white + pupil. Viewer-left is negative X.
    makePlate("EyeL_White", eyeW,
              {unit * 1.4f, unit * 1.0f, plateT},
              {-unit * 1.6f, bodyH * 0.0f + headH * 0.65f, faceZ});
    makePlate("EyeL_Pupil", eyeB,
              {unit * 0.5f, unit * 0.5f, plateT * 1.01f},
              {-unit * 1.4f, headH * 0.65f, faceZ - plateT * 0.5f});

    makePlate("EyeR_White", eyeW,
              {unit * 1.4f, unit * 1.0f, plateT},
              {+unit * 1.6f, headH * 0.65f, faceZ});
    makePlate("EyeR_Pupil", eyeB,
              {unit * 0.5f, unit * 0.5f, plateT * 1.01f},
              {+unit * 1.4f, headH * 0.65f, faceZ - plateT * 0.5f});

    // Mouth plate.
    makePlate("Mouth", mouth,
              {unit * 2.2f, unit * 0.6f, plateT},
              {0.0f, headH * 0.40f, faceZ});

    // ---- Arms (joints at shoulders, meshes offset downward) ----
    const float shoulderY = bodyH * 0.5f - unit * 1.0f; // Slightly below top of torso.
    const float shoulderX = bodyW * 0.5f + limbW * 0.5f;

    auto leftArmJoint = std::make_unique<SceneNode>("LeftArmJoint");
    leftArmJoint->transform.setLocalPosition({-shoulderX, shoulderY, 0.0f});
    m_leftArmJoint = torsoPivotPtr->addChild(std::move(leftArmJoint));

    auto leftArm = std::make_unique<SceneNode>("LeftArm");
    leftArm->mesh = &boxMesh;
    leftArm->shader = &shader;
    leftArm->tint = skin;
    leftArm->transform.setLocalScale({limbW, limbH, limbD});
    // Offset so the joint is at the shoulder while the mesh extends downward.
    leftArm->transform.setLocalPosition({0.0f, -limbH * 0.5f, 0.0f});
    m_leftArmJoint->addChild(std::move(leftArm));

    auto rightArmJoint = std::make_unique<SceneNode>("RightArmJoint");
    rightArmJoint->transform.setLocalPosition({+shoulderX, shoulderY, 0.0f});
    m_rightArmJoint = torsoPivotPtr->addChild(std::move(rightArmJoint));

    auto rightArm = std::make_unique<SceneNode>("RightArm");
    rightArm->mesh = &boxMesh;
    rightArm->shader = &shader;
    rightArm->tint = skin;
    rightArm->transform.setLocalScale({limbW, limbH, limbD});
    rightArm->transform.setLocalPosition({0.0f, -limbH * 0.5f, 0.0f});
    m_rightArmJoint->addChild(std::move(rightArm));

    // ---- Legs (joints at hip, meshes offset downward) ----
    const float hipY = -bodyH * 0.5f; // Bottom of torsoPivot.
    const float hipX = legW * 0.5f;

    auto leftLegJoint = std::make_unique<SceneNode>("LeftLegJoint");
    leftLegJoint->transform.setLocalPosition({-hipX, hipY, 0.0f});
    m_leftLegJoint = torsoPivotPtr->addChild(std::move(leftLegJoint));

    auto leftLeg = std::make_unique<SceneNode>("LeftLeg");
    leftLeg->mesh = &boxMesh;
    leftLeg->shader = &shader;
    leftLeg->tint = pants;
    leftLeg->transform.setLocalScale({legW, legH, legD});
    leftLeg->transform.setLocalPosition({0.0f, -legH * 0.5f, 0.0f});
    m_leftLegJoint->addChild(std::move(leftLeg));

    auto rightLegJoint = std::make_unique<SceneNode>("RightLegJoint");
    rightLegJoint->transform.setLocalPosition({+hipX, hipY, 0.0f});
    m_rightLegJoint = torsoPivotPtr->addChild(std::move(rightLegJoint));

    auto rightLeg = std::make_unique<SceneNode>("RightLeg");
    rightLeg->mesh = &boxMesh;
    rightLeg->shader = &shader;
    rightLeg->tint = pants;
    rightLeg->transform.setLocalScale({legW, legH, legD});
    rightLeg->transform.setLocalPosition({0.0f, -legH * 0.5f, 0.0f});
    m_rightLegJoint->addChild(std::move(rightLeg));

    // Shoes: thin darker layer at the bottom of each leg.
    auto makeShoe = [&](SceneNode* legJoint, const char* name) {
        auto s = std::make_unique<SceneNode>(name);
        s->mesh = &boxMesh;
        s->shader = &shader;
        s->tint = shoe;
        // Slight scale-up reduces visible gaps with leg mesh.
        s->transform.setLocalScale({legW * 1.02f, unit * 2.0f, legD * 1.02f});
        // Position near the foot bottom; leg mesh is centered at joint with downward offset.
        s->transform.setLocalPosition({0.0f, -legH + unit * 1.0f, 0.0f});
        legJoint->addChild(std::move(s));
    };
    makeShoe(m_leftLegJoint, "LeftShoe");
    makeShoe(m_rightLegJoint, "RightShoe");

    // Attach player root to world.
    m_playerRoot = worldRoot.addChild(std::move(playerRoot));
    return m_playerRoot;
}

/*
 * Applies a simple walking pose by rotating arm/leg joints around local +X.
 *
 * Parameters:
 *   armDeg : Arm swing angle in degrees (positive/negative swings opposite between arms).
 *   legDeg : Leg swing angle in degrees (legs swing opposite to arms).
 *
 * Notes:
 *   - This is a classic alternating gait: left arm matches right leg, right arm matches left leg.
 */
void Player::applyPose(float armDeg, float legDeg) {
    if (!m_leftArmJoint || !m_rightArmJoint || !m_leftLegJoint || !m_rightLegJoint) return;

    // Arms swing opposite.
    m_leftArmJoint->transform.setLocalRotation(RotXDeg(+armDeg));
    m_rightArmJoint->transform.setLocalRotation(RotXDeg(-armDeg));

    // Legs swing opposite to arms (classic walk).
    m_leftLegJoint->transform.setLocalRotation(RotXDeg(-legDeg));
    m_rightLegJoint->transform.setLocalRotation(RotXDeg(+legDeg));
}

/*
 * Wraps an angle in degrees to the range [-180, 180].
 *
 * Parameters:
 *   a : Angle in degrees.
 *
 * Returns:
 *   Wrapped angle in degrees.
 */
static float WrapAngleDeg(float a) {
    while (a > 180.f) a -= 360.f;
    while (a < -180.f) a += 360.f;
    return a;
}

/*
 * Clamps a scalar to the inclusive range [lo, hi].
 */
static float Clamp(float v, float lo, float hi) {
    return (v < lo) ? lo : (v > hi ? hi : v);
}

/*
 * Exponentially damps an angular value toward a target angle.
 *
 * Parameters:
 *   cur    : Current angle (degrees).
 *   target : Target angle (degrees).
 *   k      : Smoothing gain (higher = faster convergence; typical ~4 to 20).
 *   dt     : Delta time in seconds.
 *
 * Returns:
 *   New angle in degrees after exponential smoothing.
 *
 * Notes:
 *   - Uses shortest-path interpolation by wrapping the delta to [-180, 180].
 *   - Equivalent to first-order low-pass filtering in angle space.
 */
static float DampAngleDeg(float cur, float target, float k, float dt) {
    // Exponential smoothing on angles.
    float delta = WrapAngleDeg(target - cur);
    float t = 1.0f - std::exp(-k * dt);
    return cur + delta * t;
}

/*
 * Updates player movement, animation, terrain constraints, and head tracking.
 *
 * Parameters:
 *   input   : Read-only input state (WASD keys).
 *   dt      : Delta time in seconds.
 *   terrain : Heightfield/bounds provider for clamping and ground height queries.
 *   camera  : Orbit camera used to compute camera-relative movement and head look direction.
 *
 * Movement:
 *   - WASD produces forward/right axes.
 *   - Camera forward/right vectors are projected onto the ground plane (Y=0).
 *   - Movement direction is normalized and applied at m_moveSpeed.
 *
 * Animation:
 *   - When moving: sinusoidal swing driven by m_walkPhase and m_maxSwingDeg.
 *   - When idle: limbs slerp back to identity rotation (neutral pose).
 *
 * Terrain constraints:
 *   - X/Z clamped to [Min+margin, Max-margin] to prevent leaving the terrain mesh.
 *   - Y snapped to terrain height + small offset.
 *
 * Orientation:
 *   - Body yaw is set to movement direction when moving.
 *   - Head yaw is damped toward camera yaw relative to body, with clamp to a max range.
 *   - Head pitch follows camera pitch scaled and clamped.
 */
void Player::update(const Input& input, float dt, const Terrain& terrain, const Camera& camera) {
    if (!m_playerRoot) return;

    // 1) WASD axes.
    float forwardAxis = 0.f;
    float rightAxis   = 0.f;
    if (input.keyDown(GLFW_KEY_W)) forwardAxis += 1.f;
    if (input.keyDown(GLFW_KEY_S)) forwardAxis -= 1.f;
    if (input.keyDown(GLFW_KEY_D)) rightAxis   += 1.f;
    if (input.keyDown(GLFW_KEY_A)) rightAxis   -= 1.f;

    // 2) Camera-relative basis on ground plane.
    glm::vec3 camFwd = camera.forward();
    glm::vec3 camRight = camera.right();
    camFwd.y = 0.f;
    camRight.y = 0.f;

    float fLen = std::sqrt(camFwd.x*camFwd.x + camFwd.z*camFwd.z);
    float rLen = std::sqrt(camRight.x*camRight.x + camRight.z*camRight.z);

    // Fallback vectors in degenerate cases (should be rare due to camera pitch clamp).
    if (fLen < 1e-4f) camFwd = glm::vec3(0.f, 0.f, -1.f);
    else camFwd /= fLen;

    if (rLen < 1e-4f) camRight = glm::vec3(1.f, 0.f, 0.f);
    else camRight /= rLen;

    glm::vec3 moveDir = camFwd * forwardAxis + camRight * rightAxis;
    float mLen = std::sqrt(moveDir.x*moveDir.x + moveDir.z*moveDir.z);

    // 3) Camera yaw on ground (used for head look and optional idle body alignment).
    float camYawDeg = glm::degrees(std::atan2(camFwd.x, +camFwd.z));

    bool moving = (mLen > 1e-4f);

    if (moving) {
        moveDir.x /= mLen;
        moveDir.z /= mLen;

        // Body faces movement direction.
        float moveYawDeg = glm::degrees(std::atan2(moveDir.x, +moveDir.z));
        m_yawDeg = moveYawDeg;

        // Integrate position on ground plane.
        m_position += glm::vec3(moveDir.x, 0.f, moveDir.z) * (m_moveSpeed * dt);

        // Walk animation phase; scaling controls gait speed.
        m_walkPhase += m_moveSpeed * dt * 6.0f;
        float arm = std::sin(m_walkPhase) * m_maxSwingDeg;
        float leg = std::sin(m_walkPhase) * m_maxSwingDeg;
        applyPose(arm, leg);
    } else {
        // Idle: optional body alignment to camera yaw can be enabled with damping.
        // m_yawDeg = DampAngleDeg(m_yawDeg, camYawDeg, m_bodyAlignK, dt);

        // Return limbs to neutral pose smoothly (slerp to identity quaternion).
        const float k = 12.0f; // higher = faster return to rest.
        auto dampToIdentity = [&](SceneNode* joint) {
            glm::quat cur = joint->transform.localRotation();
            glm::quat id(1.f, 0.f, 0.f, 0.f);
            glm::quat nxt = glm::slerp(cur, id, 1.0f - std::exp(-k * dt));
            joint->transform.setLocalRotation(nxt);
        };
        dampToIdentity(m_leftArmJoint);
        dampToIdentity(m_rightArmJoint);
        dampToIdentity(m_leftLegJoint);
        dampToIdentity(m_rightLegJoint);
    }

    // 4) Keep inside terrain bounds (margin prevents sampling outside heightfield).
    m_position.x = std::max(terrain.MinX() + 0.2f, std::min(m_position.x, terrain.MaxX() - 0.2f));
    m_position.z = std::max(terrain.MinZ() + 0.2f, std::min(m_position.z, terrain.MaxZ() - 0.2f));

    // 5) Stick to ground heightfield.
    float groundY = terrain.GetHeight(m_position.x, m_position.z);
    m_position.y = groundY + 0.02f; // small lift to avoid Z-fighting with terrain surface.

    // 6) Apply body transform.
    m_playerRoot->transform.setLocalPosition(m_position);
    m_playerRoot->transform.setLocalRotation(RotYDeg(m_yawDeg));

    // 7) Head looks where camera looks (relative to body).
    if (m_headJoint) {
        // Desired head yaw is camera yaw relative to body yaw.
        float targetHeadYaw = WrapAngleDeg(camYawDeg - m_yawDeg);
        targetHeadYaw = Clamp(targetHeadYaw, -m_headMaxYawDeg, m_headMaxYawDeg);

        // Separate responsiveness when moving vs idle.
        float k = moving ? m_headYawKMoving : m_headYawKIdle;
        m_headYawDeg = DampAngleDeg(m_headYawDeg, targetHeadYaw, k, dt);

        // Head pitch follows camera pitch with scaling and clamp.
        float headPitch = Clamp(camera.pitchDeg * m_headPitchScale,
                                -m_headMaxPitchDeg, m_headMaxPitchDeg);

        // Apply combined yaw then pitch in local joint space.
        m_headJoint->transform.setLocalRotation(RotYDeg(m_headYawDeg) * RotXDeg(headPitch));
    }
}
