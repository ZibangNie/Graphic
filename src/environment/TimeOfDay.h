/*
* TimeOfDay.h
 *
 * Purpose:
 *   Declares a minimal time-of-day simulator that tracks a normalized day cycle.
 *   Provides both normalized time and an hours-of-day convenience accessor.
 *
 * Model:
 *   - normalizedTime() returns m_time01 in [0,1), where 0 and 1 represent the same time of day.
 *   - hours() maps normalized time into a 24-hour clock representation.
 *
 * Notes:
 *   - The initial value of m_time01 sets the starting time-of-day at application launch.
 */

#pragma once

class TimeOfDay {
public:
    /*
     * Advances the time-of-day accumulator.
     *
     * Parameters:
     *   dt : Delta time in seconds. Expected to be non-negative.
     */
    void update(float dt);

    // Normalized time-of-day in [0,1).
    float normalizedTime() const { return m_time01; }

    // Convenience conversion to "clock hours" in [0,24).
    float hours() const { return m_time01 * 24.0f; }

private:
    // Normalized time-of-day. Default initializes to morning (~0.25 => ~06:00).
    float m_time01 = 0.25f;
};
