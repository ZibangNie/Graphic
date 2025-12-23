/*
* Sun.h
 *
 * Purpose:
 *   Declares a simple Sun model that produces a DirectionalLight derived from TimeOfDay.
 *   The resulting light provides direction, color, and intensity for scene lighting and sky rendering.
 *
 * Types:
 *   - DirectionalLight: Minimal directional light representation used by the lighting system/shaders.
 *
 * Notes:
 *   - Direction convention should be consistent across the engine (e.g., whether direction points toward the sun
 *     or represents incoming light direction). Shaders typically assume a consistent N·L usage.
 */

#pragma once
#include <glm/glm.hpp>
#include "TimeOfDay.h"

/*
 * Minimal directional light representation.
 *
 * Fields:
 *   direction : World-space direction vector. Expected to be normalized by the producer.
 *   color     : Light color in linear RGB (typically in [0,1], may exceed for stylized/HDR pipelines).
 *   intensity : Scalar brightness multiplier. Typical range depends on exposure/tone mapping.
 */
struct DirectionalLight {
    glm::vec3 direction;
    glm::vec3 color;
    float intensity;
};

class Sun {
public:
    /*
     * Updates the sun's directional light parameters based on time-of-day.
     *
     * Parameters:
     *   time : TimeOfDay simulation state providing normalized time.
     */
    void update(const TimeOfDay& time);

    // Read-only access to the current directional light state.
    const DirectionalLight& light() const { return m_light; }

private:
    DirectionalLight m_light;
};
