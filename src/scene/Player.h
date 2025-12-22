#pragma once

#include <memory>
#include <glm/glm.hpp>

class Input;
class SceneNode;
class Mesh;
class Shader;
class Terrain;
class Camera;

class Player {
public:
    Player() = default;

    // 构建 Steve 节点树，并挂到 worldRoot 下
    // 返回 playerRoot 指针（由 worldRoot 持有生命周期）
    SceneNode* build(SceneNode& worldRoot, Mesh& boxMesh, Shader& shader);

    // 更新移动与动画
    void update(const Input& input, float dt, const Terrain& terrain, const Camera& camera);

    glm::vec3 position() const { return m_position; }
    SceneNode* rootNode() const { return m_playerRoot; }

private:
    void applyPose(float armDeg, float legDeg);

private:
    SceneNode* m_playerRoot = nullptr;

    // joints (no mesh)
    SceneNode* m_leftArmJoint = nullptr;
    SceneNode* m_rightArmJoint = nullptr;
    SceneNode* m_leftLegJoint = nullptr;
    SceneNode* m_rightLegJoint = nullptr;

    glm::vec3 m_position{0.f, 0.f, 0.f};
    float m_yawDeg = 0.f;

    float m_walkPhase = 0.f;
    float m_moveSpeed = 2.5f;      // units/s
    float m_maxSwingDeg = 35.0f;   // limb swing angle

    SceneNode* m_headJoint = nullptr;

    // Tuning
    float m_bodyAlignK = 6.0f;        // idle时身体对齐相机的速度(越大越快)
    float m_headMaxYawDeg = 70.0f;    // 头部最大左右扭转角
    float m_headPitchScale = 0.35f;   // 头部跟随相机俯仰的比例(可设0禁用pitch)
    float m_headMaxPitchDeg = 25.0f;  // 头部最大上下俯仰角

    float m_headYawDeg = 0.0f;       // current head yaw offset (relative to body)
    float m_headYawKMoving = 18.0f;  // moving: fast follow camera
    float m_headYawKIdle   = 4.0f;   // idle: slow follow camera
};
