/*
 * Sun.cpp
 *
 * Purpose:
 *   Implements Sun time-of-day behavior by deriving a directional light (direction, color, intensity)
 *   from the normalized time provided by TimeOfDay.
 *
 * Model:
 *   - The sun follows a simple periodic path around the scene based on normalized time.
 *   - Light direction is computed from a parametric angle.
 *   - Light color shifts warmer near the horizon (sunrise/sunset) and whiter near noon.
 *   - Light intensity ramps strongly with sun elevation to make nights darker and midday brighter.
 *
 * Notes:
 *   - This is an artistic/approximate model rather than a physically accurate sun/atmosphere simulation.
 *   - Direction convention assumes downstream shading uses N·L where L points toward the sun.
 */

#include "Sun.h"
#include <glm/gtc/constants.hpp>
#include <cmath>

/*
 * Updates the sun light parameters from the current time-of-day.
 *
 * Parameters:
 *   time : TimeOfDay state providing normalizedTime() in [0,1).
 *
 * Behavior:
 *   - Computes a sun angle such that t≈0.25 corresponds to sunrise at the horizon.
 *   - Updates m_light.direction as a normalized world-space vector.
 *   - Computes "day" factor from sun elevation (Y component) to drive intensity.
 *   - Computes "horizon" factor (strong near sunrise/sunset) to drive color warmth.
 *
 * Notes:
 *   - The exact mapping between normalized time and sunrise/noon/sunset depends on the (t - 0.25) phase shift.
 *   - day and horizon use squared ramps to increase contrast (brighter days, darker nights, warmer horizon).
 */
void Sun::update(const TimeOfDay& time) {
    float t = time.normalizedTime();

    // Sun angle (radians). Phase shift places sunrise near t = 0.25.
    float angle = (t - 0.25f) * glm::two_pi<float>();

    // Parametric direction; normalized to ensure stable shading.
    // X uses cos(angle); Y/Z use sin(angle) to create a simple rising/setting arc.
    m_light.direction = glm::normalize(glm::vec3(
        std::cos(angle),
        std::sin(angle),
        std::sin(angle)
    ));

    float sunY = m_light.direction.y; // Elevation in [-1, 1].

    // Day factor from sun elevation:
    //   - Clamped ramp to emphasize darker nights.
    //   - Squared for higher contrast (steeper transition).
    float day = glm::clamp((sunY - 0.02f) / 0.35f, 0.0f, 1.0f);
    day = day * day;

    // Horizon factor:
    //   - High near sunrise/sunset where |sunY| is small.
    //   - Squared for a stronger warm-color contribution near the horizon.
    float horizon = 1.0f - glm::clamp(std::abs(sunY) / 0.25f, 0.0f, 1.0f);
    horizon = horizon * horizon;

    glm::vec3 noon(1.0f, 0.97f, 0.92f);
    glm::vec3 dusk(1.0f, 0.45f, 0.20f);

    // Color: whiter near noon, warmer near sunrise/sunset.
    m_light.color = glm::mix(noon, dusk, 0.75f * horizon);

    // Intensity: near-zero at night, significantly stronger during the day.
    // Baseline 0.1f avoids a fully black sun term; adjust to taste.
    m_light.intensity = 0.1f + 2.2f * day;
}
