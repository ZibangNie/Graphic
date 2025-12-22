#include "scene/Camera.h"
#include "core/Input.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

float Camera::clamp(float v, float lo, float hi) {
    return (v < lo) ? lo : (v > hi ? hi : v);
}

// 与你之前自由相机 forward 公式一致：yaw=-90 -> -Z
glm::vec3 Camera::forwardFromYawPitch() const {
    float yaw   = glm::radians(yawDeg);
    float pitch = glm::radians(pitchDeg);

    glm::vec3 f;
    f.x = std::cos(yaw) * std::cos(pitch);
    f.y = std::sin(pitch);
    f.z = std::sin(yaw) * std::cos(pitch);
    return glm::normalize(f);
}

void Camera::updateOrbit(Input& input, const glm::vec3& targetWorldPos) {
    // 1) Zoom by scroll (always available)
    double scroll = input.consumeScrollY();
    if (scroll != 0.0) {
        distance -= static_cast<float>(scroll) * zoomSpeed;
        distance = clamp(distance, minDistance, maxDistance);
    }

    // 2) Rotate only when holding RMB
    if (input.mouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT)) {
        yawDeg   += static_cast<float>(input.mouseDeltaX()) * mouseSensitivity;
        pitchDeg -= static_cast<float>(input.mouseDeltaY()) * mouseSensitivity;
        pitchDeg = clamp(pitchDeg, minPitchDeg, maxPitchDeg);
    }

    // 3) Compute pivot & camera position
    pivot = targetWorldPos + glm::vec3(0.f, height, 0.f);

    // forward points from camera towards pivot, so camera is behind pivot along -forward
    glm::vec3 fwd = forwardFromYawPitch();
    position = pivot - fwd * distance;
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(position, pivot, glm::vec3(0, 1, 0));
}

glm::vec3 Camera::forward() const {
    glm::vec3 f = pivot - position; // from camera to pivot
    float len = std::sqrt(f.x*f.x + f.y*f.y + f.z*f.z);
    if (len < 1e-6f) return glm::vec3(0.f, 0.f, -1.f);
    return f / len;
}

glm::vec3 Camera::right() const {
    glm::vec3 f = forward();
    glm::vec3 r = glm::cross(f, glm::vec3(0.f, 1.f, 0.f));
    float len = std::sqrt(r.x*r.x + r.y*r.y + r.z*r.z);
    if (len < 1e-6f) return glm::vec3(1.f, 0.f, 0.f);
    return r / len;
}

void Camera::updateOrbitNoInput(const glm::vec3& targetWorldPos) {
    pivot = targetWorldPos + glm::vec3(0.f, height, 0.f);
    glm::vec3 fwd = forwardFromYawPitch();
    position = pivot - fwd * distance;
}

