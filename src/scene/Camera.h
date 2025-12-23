/*
 * Camera.h
 *
 * Purpose:
 *   Declares a third-person orbit camera used to follow a target (e.g., the player).
 *   The camera maintains yaw/pitch angles and a distance from a pivot point derived from the target.
 *
 * Camera model:
 *   - pivot = targetWorldPos + (0, height, 0)
 *   - position = pivot - forward(yaw,pitch) * distance
 *   - view matrix = lookAt(position, pivot, worldUp)
 *
 * Coordinate conventions:
 *   - World up is +Y.
 *   - yawDeg/pitchDeg are stored in degrees.
 *   - forward() returns the viewing direction from camera position towards pivot.
 *
 * Input mapping (in implementation):
 *   - Mouse wheel: zoom (distance) with clamping to [minDistance, maxDistance]
 *   - RMB + mouse delta: orbit rotation with pitch clamping to [minPitchDeg, maxPitchDeg]
 *
 * Parameter guidance:
 *   - distance: typical third-person values in the range ~[3, 10]
 *   - height: pivot lift to frame the character head/torso (commonly ~[1.0, 2.0])
 *   - pitch limits: prevent flipping and excessive top-down views
 */

#pragma once

#include <glm/glm.hpp>

class Input;

class Camera {
public:
    // Orbit angles (degrees).
    // yawDeg: horizontal orbit around world up (+Y).
    // pitchDeg: vertical orbit angle (positive looks downward less / upward more depending on convention).
    float yawDeg   = 180.0f;   // Initial yaw places camera behind the character.
    float pitchDeg = 20.0f;

    // Orbit distance from pivot (world units).
    float distance    = 6.0f;
    float minDistance = 2.0f;
    float maxDistance = 12.0f;

    // Pivot height offset above targetWorldPos (world units).
    // Larger values raise the look-at point to keep the character in frame.
    float height = 1.3f;

    // Vertical rotation limits (degrees) to prevent flipping and extreme angles.
    float minPitchDeg = -20.0f;
    float maxPitchDeg = 75.0f;

    // Input sensitivity tuning.
    // mouseSensitivity: degrees per pixel of mouse delta (typical ~0.05 to 0.3).
    // zoomSpeed: distance delta per scroll "tick" (typical ~0.2 to 1.0).
    float mouseSensitivity = 0.12f;
    float zoomSpeed = 0.6f; // scroll tick -> distance delta

    // Computed state updated each frame.
    glm::vec3 position{0.f}; // Camera world position.
    glm::vec3 pivot{0.f};    // Look-at target position (pivot point).

    /*
     * Updates orbit camera around the given target position using input.
     *
     * Parameters:
     *   input          : Per-frame input state (mouse delta, scroll, mouse buttons).
     *   targetWorldPos : Target position in world space to orbit around.
     *
     * Notes:
     *   - Consumes scroll input each frame.
     *   - Applies pitch/distance clamping.
     */
    void updateOrbit(Input& input, const glm::vec3& targetWorldPos);

    /*
     * Returns the view matrix for the current camera state.
     *
     * Returns:
     *   View matrix (world -> view space).
     */
    glm::mat4 getViewMatrix() const;

    /*
     * Returns camera basis vectors in world space.
     *
     * forward(): viewing direction (from camera position towards pivot).
     * right(): right direction derived from forward and world up (+Y).
     */
    glm::vec3 forward() const; // direction camera looks (from camera -> pivot)
    glm::vec3 right() const;   // camera right on world XZ

    /*
     * Recomputes pivot and position without consuming input.
     *
     * Use cases:
     *   - Re-center camera after the target moved within the same frame.
     *   - Maintain camera lock to target when orbit parameters remain unchanged.
     */
    void updateOrbitNoInput(const glm::vec3& targetWorldPos);

private:
    // Scalar clamp helper (inclusive range).
    static float clamp(float v, float lo, float hi);

    // Computes normalized forward direction from yawDeg/pitchDeg (degrees).
    glm::vec3 forwardFromYawPitch() const; // where camera looks
};
