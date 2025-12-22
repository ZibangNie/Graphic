#pragma once

#include <glm/glm.hpp>

class Input;

class Camera {
public:
    // Orbit angles (degrees)
    float yawDeg   = 180.0f;   // 初始在角色背后
    float pitchDeg = 20.0f;

    // Orbit distance
    float distance    = 6.0f;
    float minDistance = 2.0f;
    float maxDistance = 12.0f;

    // Pivot height above target (look-at point)
    float height = 1.3f;

    // Rotation limits
    float minPitchDeg = -20.0f;
    float maxPitchDeg = 75.0f;

    // Sensitivity
    float mouseSensitivity = 0.12f;
    float zoomSpeed = 0.6f; // scroll tick -> distance delta

    // Computed each update
    glm::vec3 position{0.f};
    glm::vec3 pivot{0.f}; // look-at target position

    // Update orbit camera around targetWorldPos
    void updateOrbit(Input& input, const glm::vec3& targetWorldPos);

    // View matrix
    glm::mat4 getViewMatrix() const;

    // Camera basis in world space
    glm::vec3 forward() const; // direction camera looks (from camera -> pivot)
    glm::vec3 right() const;   // camera right on world XZ

    // Recompute pivot/position without consuming input (useful after player moved)
    void updateOrbitNoInput(const glm::vec3& targetWorldPos);


private:
    static float clamp(float v, float lo, float hi);
    glm::vec3 forwardFromYawPitch() const; // where camera looks
};

