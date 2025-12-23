/*
 * Camera.cpp
 *
 * Purpose:
 *   Implements a third-person orbit camera:
 *     - Camera orbits around a pivot point (typically the player position + height offset).
 *     - Mouse wheel controls zoom (distance).
 *     - Right mouse button (RMB) drag controls yaw/pitch rotation.
 *
 * Coordinate conventions:
 *   - World up is +Y.
 *   - Yaw/pitch are stored in degrees (yawDeg, pitchDeg).
 *   - forwardFromYawPitch() follows the common FPS convention where yaw = -90° faces -Z.
 *
 * Runtime behavior:
 *   - updateOrbit() consumes per-frame input:
 *       1) zoom (scroll)
 *       2) rotation (RMB + mouse delta)
 *       3) recompute pivot/position
 *   - updateOrbitNoInput() recomputes pivot/position without consuming input,
 *     used when the target moves but camera parameters remain unchanged.
 */

#include "scene/Camera.h"
#include <GLFW/glfw3.h>
#include "core/Input.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

/*
 * Clamps a scalar value to the inclusive range [lo, hi].
 *
 * Parameters:
 *   v  : Input value.
 *   lo : Lower bound.
 *   hi : Upper bound.
 *
 * Returns:
 *   Clamped value.
 */
float Camera::clamp(float v, float lo, float hi) {
    return (v < lo) ? lo : (v > hi ? hi : v);
}

/*
 * Computes the unit forward direction from yaw/pitch (degrees).
 *
 * Returns:
 *   Normalized forward vector in world space.
 *
 * Notes:
 *   - This forward vector points in the direction the camera "looks" (from camera towards pivot).
 *   - Convention matches typical FPS math:
 *       yaw = -90° => forward = (0, 0, -1)
 *   - pitch is clamped elsewhere to avoid gimbal lock and extreme angles.
 */
glm::vec3 Camera::forwardFromYawPitch() const {
    float yaw   = glm::radians(yawDeg);
    float pitch = glm::radians(pitchDeg);

    glm::vec3 f;
    f.x = std::cos(yaw) * std::cos(pitch);
    f.y = std::sin(pitch);
    f.z = std::sin(yaw) * std::cos(pitch);
    return glm::normalize(f);
}

/*
 * Updates orbit camera using user input and a target world position.
 *
 * Parameters:
 *   input          : Input state provider (mouse delta, RMB state, scroll).
 *   targetWorldPos : Target position to orbit around (e.g., player position).
 *
 * Controls:
 *   - Mouse wheel: adjusts distance (zoom) by zoomSpeed.
 *   - RMB drag: changes yaw/pitch by mouseSensitivity.
 *
 * Notes:
 *   - Scroll is consumed each frame via consumeScrollY().
 *   - pitchDeg is clamped to [minPitchDeg, maxPitchDeg] to avoid flipping.
 *   - distance is clamped to [minDistance, maxDistance].
 */
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
    // pivot is offset upward so the camera looks slightly above the target's origin.
    pivot = targetWorldPos + glm::vec3(0.f, height, 0.f);

    // forward points from camera towards pivot, so camera is behind pivot along -forward.
    glm::vec3 fwd = forwardFromYawPitch();
    position = pivot - fwd * distance;
}

/*
 * Builds the view matrix for the current camera state.
 *
 * Returns:
 *   View matrix (world -> view space) using lookAt(position, pivot, up).
 *
 * Notes:
 *   - World up is fixed as +Y.
 */
glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(position, pivot, glm::vec3(0, 1, 0));
}

/*
 * Returns the camera forward direction (from camera position to pivot).
 *
 * Returns:
 *   Normalized vector pointing from the camera towards the pivot.
 *
 * Edge cases:
 *   - If position equals pivot (degenerate), returns a default forward (-Z).
 */
glm::vec3 Camera::forward() const {
    glm::vec3 f = pivot - position; // from camera to pivot
    float len = std::sqrt(f.x*f.x + f.y*f.y + f.z*f.z);
    if (len < 1e-6f) return glm::vec3(0.f, 0.f, -1.f);
    return f / len;
}

/*
 * Returns the camera right direction in world space.
 *
 * Returns:
 *   Normalized right vector computed as cross(forward, worldUp).
 *
 * Edge cases:
 *   - If forward is degenerate/parallel to up (rare due to pitch clamp), returns +X.
 */
glm::vec3 Camera::right() const {
    glm::vec3 f = forward();
    glm::vec3 r = glm::cross(f, glm::vec3(0.f, 1.f, 0.f));
    float len = std::sqrt(r.x*r.x + r.y*r.y + r.z*r.z);
    if (len < 1e-6f) return glm::vec3(1.f, 0.f, 0.f);
    return r / len;
}

/*
 * Updates orbit camera pivot/position without consuming input.
 *
 * Parameters:
 *   targetWorldPos : Target position to orbit around.
 *
 * Use cases:
 *   - Keep the camera centered on a moving target when input has already been consumed.
 *   - Re-center after the target position updates within the same frame.
 */
void Camera::updateOrbitNoInput(const glm::vec3& targetWorldPos) {
    pivot = targetWorldPos + glm::vec3(0.f, height, 0.f);
    glm::vec3 fwd = forwardFromYawPitch();
    position = pivot - fwd * distance;
}
