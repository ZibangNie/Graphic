/*
* TimeOfDay.cpp
 *
 * Purpose:
 *   Implements the TimeOfDay simulation update step.
 *   Advances a normalized time accumulator over a fixed-length day/night cycle.
 *
 * Model:
 *   - m_time01 is maintained in the range [0, 1).
 *   - A full cycle (0 -> 1) corresponds to one in-game "day".
 */

#include "TimeOfDay.h"

/*
 * Advances the time-of-day accumulator.
 *
 * Parameters:
 *   dt : Delta time in seconds. Expected to be non-negative.
 *
 * Behavior:
 *   - Increments normalized time by dt / dayLengthSeconds.
 *   - Wraps back into [0,1) when exceeding 1.0.
 *
 * Notes:
 *   - dayLengthSeconds controls the simulated day duration. Smaller values speed up the day/night cycle.
 */
void TimeOfDay::update(float dt) {
    constexpr float dayLengthSeconds = 30.0f; // Seconds per full day/night cycle.
    m_time01 += dt / dayLengthSeconds;
    if (m_time01 > 1.0f)
        m_time01 -= 1.0f;
}
