/*
 * Player.h
 *
 * Purpose:
 *   Declares a Minecraft-style player entity implemented as a SceneNode hierarchy.
 *   Responsibilities include:
 *     - Building the "Steve" node tree (body parts + joints) under a world root
 *     - Updating camera-relative movement on the ground plane
 *     - Driving a simple walking animation and head look behavior
 *
 * Ownership / lifetime:
 *   - build() attaches a newly created PlayerRoot under worldRoot.
 *   - The returned SceneNode* is owned by worldRoot (Player stores non-owning pointers).
 *
 * Update model:
 *   - update() reads input (WASD), computes movement direction relative to the camera,
 *     clamps motion to terrain bounds, snaps Y to terrain height, and updates node transforms.
 */

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

    /*
     * Builds the Steve-like scene graph and attaches it to worldRoot.
     *
     * Parameters:
     *   worldRoot : World scene root that will own the created node subtree.
     *   boxMesh   : Unit cube mesh used for all body parts (scaled per node).
     *   shader    : Shader assigned to each body part node.
     *
     * Returns:
     *   Pointer to the created PlayerRoot node (lifetime managed by worldRoot).
     */
    SceneNode* build(SceneNode& worldRoot, Mesh& boxMesh, Shader& shader);

    /*
     * Updates player movement and animation.
     *
     * Parameters:
     *   input   : Input state (WASD) for movement.
     *   dt      : Delta time in seconds.
     *   terrain : Terrain provider for bounds clamping and ground height sampling.
     *   camera  : Camera used to compute camera-relative movement and head look direction.
     *
     * Behavior summary:
     *   - Movement is computed on the XZ plane using camera forward/right.
     *   - When moving, body yaw faces movement direction and limbs swing.
     *   - When idle, limbs return to neutral and head yaw follows camera more slowly.
     */
    void update(const Input& input, float dt, const Terrain& terrain, const Camera& camera);

    // Current world-space player position (feet snapped to terrain height in update()).
    glm::vec3 position() const { return m_position; }

    // Root SceneNode for the player hierarchy (non-owning pointer; owned by worldRoot).
    SceneNode* rootNode() const { return m_playerRoot; }

private:
    /*
     * Applies a walking pose by rotating limb joints around local +X.
     *
     * Parameters:
     *   armDeg : Arm swing angle in degrees.
     *   legDeg : Leg swing angle in degrees.
     *
     * Notes:
     *   - Arms swing opposite each other; legs swing opposite arms for a basic gait.
     */
    void applyPose(float armDeg, float legDeg);

private:
    // Player root node in the scene graph (non-owning).
    SceneNode* m_playerRoot = nullptr;

    // Joint nodes (no mesh). Rotations are applied here to move attached limb meshes.
    SceneNode* m_leftArmJoint = nullptr;
    SceneNode* m_rightArmJoint = nullptr;
    SceneNode* m_leftLegJoint = nullptr;
    SceneNode* m_rightLegJoint = nullptr;

    // World-space position and body yaw (degrees).
    glm::vec3 m_position{0.f, 0.f, 0.f};
    float m_yawDeg = 0.f;

    // Walk cycle state.
    float m_walkPhase = 0.f;
    float m_moveSpeed = 2.5f;      // Movement speed in world units per second.
    float m_maxSwingDeg = 35.0f;   // Max limb swing amplitude (degrees).

    // Head joint node (no mesh). Used for head yaw/pitch.
    SceneNode* m_headJoint = nullptr;

    // Tuning parameters:
    // m_bodyAlignK: optional idle body yaw alignment toward camera (exponential smoothing gain).
    // m_headMaxYawDeg: clamp range for head yaw relative to body.
    // m_headPitchScale: scale factor mapping camera pitch to head pitch (0 disables pitch follow).
    // m_headMaxPitchDeg: clamp range for head pitch.
    float m_bodyAlignK = 6.0f;        // Idle body alignment speed (higher = faster).
    float m_headMaxYawDeg = 70.0f;    // Max left/right head yaw (degrees).
    float m_headPitchScale = 0.35f;   // Head pitch response to camera pitch (0 to disable).
    float m_headMaxPitchDeg = 25.0f;  // Max up/down head pitch (degrees).

    // Head yaw state and responsiveness.
    // m_headYawDeg: current head yaw offset relative to body yaw.
    // m_headYawKMoving: smoothing gain when moving (higher = faster).
    // m_headYawKIdle: smoothing gain when idle (lower = slower).
    float m_headYawDeg = 0.0f;       // Current head yaw offset (relative to body).
    float m_headYawKMoving = 18.0f;  // Moving: fast follow camera.
    float m_headYawKIdle   = 4.0f;   // Idle: slow follow camera.
};
