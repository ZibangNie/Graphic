#include "scene/Player.h"
#include "scene/SceneNode.h"
#include "core/Input.h"
#include "scene/Terrain.h"
#include "scene/Camera.h"

#include <glm/gtc/quaternion.hpp>

#include <cmath>



static glm::quat RotXDeg(float deg) {
    return glm::angleAxis(glm::radians(deg), glm::vec3(1,0,0));
}
static glm::quat RotYDeg(float deg) {
    return glm::angleAxis(glm::radians(deg), glm::vec3(0,1,0));
}

SceneNode* Player::build(SceneNode& worldRoot, Mesh& boxMesh, Shader& shader) {
    // ----- scale unit (Minecraft "pixel") -----
    // Minecraft proportions: head 8, body 12, limbs 12 (in pixels)
    const float unit = 0.10f; // tune overall size here

    const float headW = 8 * unit, headH = 8 * unit, headD = 8 * unit;
    const float bodyW = 8 * unit, bodyH = 12 * unit, bodyD = 4 * unit;
    const float limbW = 4 * unit, limbH = 12 * unit, limbD = 4 * unit;
    const float legW  = 4 * unit, legH  = 12 * unit, legD  = 4 * unit;

    // ----- colors close to Steve -----
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
    // place torso so that feet are on ground (y=0)
    // Legs length = legH, torso height = bodyH. Torso center at legH + bodyH/2
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
    headJoint->transform.setLocalPosition({0.0f, bodyH * 0.5f, 0.0f}); // neck point relative to torsoPivot
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
    hairLayer->transform.setLocalScale({headW * 1.04f, headH * 1.04f, headD * 1.04f});
    hairLayer->transform.setLocalPosition({0.0f, headH * 0.5f, 0.0f});
    headJointPtr->addChild(std::move(hairLayer));

    // Face details: thin quads (actually very thin boxes) on the front of head
    const float faceZ = headD * 0.5f + unit * 0.50f;
    const float plateT = unit * 0.04f;               // thickness

    auto makePlate = [&](const char* name, glm::vec3 tint, glm::vec3 scale, glm::vec3 pos) {
        auto n = std::make_unique<SceneNode>(name);
        n->mesh = &boxMesh;
        n->shader = &shader;
        n->tint = tint;
        n->transform.setLocalScale(scale);
        n->transform.setLocalPosition(pos);
        headJointPtr->addChild(std::move(n));
    };

    // Eyes: white + pupil
    // left eye (Steve viewer-left is negative X)
    makePlate("EyeL_White", eyeW,
              {unit * 1.4f, unit * 1.0f, plateT},
              {-unit * 1.6f, bodyH * 0.0f + headH * 0.65f, faceZ});
    makePlate("EyeL_Pupil", eyeB,
              {unit * 0.5f, unit * 0.5f, plateT * 1.01f},
              {-unit * 1.4f, headH * 0.65f, faceZ - plateT * 0.5f});

    // right eye
    makePlate("EyeR_White", eyeW,
              {unit * 1.4f, unit * 1.0f, plateT},
              {+unit * 1.6f, headH * 0.65f, faceZ});
    makePlate("EyeR_Pupil", eyeB,
              {unit * 0.5f, unit * 0.5f, plateT * 1.01f},
              {+unit * 1.4f, headH * 0.65f, faceZ - plateT * 0.5f});

    // Mouth
    makePlate("Mouth", mouth,
              {unit * 2.2f, unit * 0.6f, plateT},
              {0.0f, headH * 0.40f, faceZ});

    // ---- Arms (joints at shoulders, meshes offset downward) ----
    const float shoulderY = bodyH * 0.5f - unit * 1.0f; // slightly below top
    const float shoulderX = bodyW * 0.5f + limbW * 0.5f;

    auto leftArmJoint = std::make_unique<SceneNode>("LeftArmJoint");
    leftArmJoint->transform.setLocalPosition({-shoulderX, shoulderY, 0.0f});
    m_leftArmJoint = torsoPivotPtr->addChild(std::move(leftArmJoint));

    auto leftArm = std::make_unique<SceneNode>("LeftArm");
    leftArm->mesh = &boxMesh;
    leftArm->shader = &shader;
    leftArm->tint = skin;
    leftArm->transform.setLocalScale({limbW, limbH, limbD});
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
    const float hipY = -bodyH * 0.5f; // bottom of torsoPivot
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

    // Shoes (optional): thin darker layer at the bottom of each leg
    auto makeShoe = [&](SceneNode* legJoint, const char* name) {
        auto s = std::make_unique<SceneNode>(name);
        s->mesh = &boxMesh;
        s->shader = &shader;
        s->tint = shoe;
        s->transform.setLocalScale({legW * 1.02f, unit * 2.0f, legD * 1.02f});
        s->transform.setLocalPosition({0.0f, -legH + unit * 1.0f, 0.0f});
        legJoint->addChild(std::move(s));
    };
    makeShoe(m_leftLegJoint, "LeftShoe");
    makeShoe(m_rightLegJoint, "RightShoe");

    // Attach player root to world
    m_playerRoot = worldRoot.addChild(std::move(playerRoot));
    return m_playerRoot;
}


void Player::applyPose(float armDeg, float legDeg) {
    if (!m_leftArmJoint || !m_rightArmJoint || !m_leftLegJoint || !m_rightLegJoint) return;

    // Arms swing opposite
    m_leftArmJoint->transform.setLocalRotation(RotXDeg(+armDeg));
    m_rightArmJoint->transform.setLocalRotation(RotXDeg(-armDeg));

    // Legs swing opposite to arms (classic walk)
    m_leftLegJoint->transform.setLocalRotation(RotXDeg(-legDeg));
    m_rightLegJoint->transform.setLocalRotation(RotXDeg(+legDeg));
}

static float WrapAngleDeg(float a) {
    while (a > 180.f) a -= 360.f;
    while (a < -180.f) a += 360.f;
    return a;
}
static float Clamp(float v, float lo, float hi) {
    return (v < lo) ? lo : (v > hi ? hi : v);
}
static float DampAngleDeg(float cur, float target, float k, float dt) {
    // exponential smoothing on angles
    float delta = WrapAngleDeg(target - cur);
    float t = 1.0f - std::exp(-k * dt);
    return cur + delta * t;
}

void Player::update(const Input& input, float dt, const Terrain& terrain, const Camera& camera) {
    if (!m_playerRoot) return;

    // 1) WASD axes
    float forwardAxis = 0.f;
    float rightAxis   = 0.f;
    if (input.keyDown(GLFW_KEY_W)) forwardAxis += 1.f;
    if (input.keyDown(GLFW_KEY_S)) forwardAxis -= 1.f;
    if (input.keyDown(GLFW_KEY_D)) rightAxis   += 1.f;
    if (input.keyDown(GLFW_KEY_A)) rightAxis   -= 1.f;

    // 2) Camera-relative basis on ground plane
    glm::vec3 camFwd = camera.forward();
    glm::vec3 camRight = camera.right();
    camFwd.y = 0.f;
    camRight.y = 0.f;

    float fLen = std::sqrt(camFwd.x*camFwd.x + camFwd.z*camFwd.z);
    float rLen = std::sqrt(camRight.x*camRight.x + camRight.z*camRight.z);

    if (fLen < 1e-4f) camFwd = glm::vec3(0.f, 0.f, -1.f);
    else camFwd /= fLen;

    if (rLen < 1e-4f) camRight = glm::vec3(1.f, 0.f, 0.f);
    else camRight /= rLen;

    glm::vec3 moveDir = camFwd * forwardAxis + camRight * rightAxis;
    float mLen = std::sqrt(moveDir.x*moveDir.x + moveDir.z*moveDir.z);

    // 3) Camera yaw on ground (for idle facing / head look)
    float camYawDeg = glm::degrees(std::atan2(camFwd.x, +camFwd.z));

    bool moving = (mLen > 1e-4f);

    if (moving) {
        moveDir.x /= mLen;
        moveDir.z /= mLen;

        // Body faces movement direction (instant / could damp if you want)
        float moveYawDeg = glm::degrees(std::atan2(moveDir.x, +moveDir.z));
        m_yawDeg = moveYawDeg;

        m_position += glm::vec3(moveDir.x, 0.f, moveDir.z) * (m_moveSpeed * dt);

        // walk animation
        m_walkPhase += m_moveSpeed * dt * 6.0f;
        float arm = std::sin(m_walkPhase) * m_maxSwingDeg;
        float leg = std::sin(m_walkPhase) * m_maxSwingDeg;
        applyPose(arm, leg);
    } else {
        // Idle: body slowly aligns to camera yaw (so "character facing follows camera")
        //m_yawDeg = DampAngleDeg(m_yawDeg, camYawDeg, m_bodyAlignK, dt);

        // return limbs to neutral smoothly
        const float k = 12.0f;
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

    // 4) Keep inside terrain bounds
    m_position.x = std::max(terrain.MinX() + 0.2f, std::min(m_position.x, terrain.MaxX() - 0.2f));
    m_position.z = std::max(terrain.MinZ() + 0.2f, std::min(m_position.z, terrain.MaxZ() - 0.2f));

    // 5) Stick to ground
    float groundY = terrain.GetHeight(m_position.x, m_position.z);
    m_position.y = groundY + 0.02f;

    // 6) Apply body transform
    m_playerRoot->transform.setLocalPosition(m_position);
    m_playerRoot->transform.setLocalRotation(RotYDeg(m_yawDeg));

    // 7) Head looks where camera looks (relative to body)
    if (m_headJoint) {
        float targetHeadYaw = WrapAngleDeg(camYawDeg - m_yawDeg);
        targetHeadYaw = Clamp(targetHeadYaw, -m_headMaxYawDeg, m_headMaxYawDeg);

        float k = moving ? m_headYawKMoving : m_headYawKIdle;
        m_headYawDeg = DampAngleDeg(m_headYawDeg, targetHeadYaw, k, dt);

        // head pitch follows camera pitch (scaled + clamped)
        float headPitch = Clamp(camera.pitchDeg * m_headPitchScale,
                                -m_headMaxPitchDeg, m_headMaxPitchDeg);

        m_headJoint->transform.setLocalRotation(RotYDeg(m_headYawDeg) * RotXDeg(headPitch));
    }
}
