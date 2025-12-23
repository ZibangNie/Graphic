/*
* Environment.h
 *
 * Purpose:
 *   Declares the Environment facade that aggregates time-of-day and sun state.
 *   Provides a single update entry point and read-only accessors for dependent systems.
 *
 * Responsibilities:
 *   - Advance TimeOfDay each frame.
 *   - Update Sun parameters (direction, intensity, color, etc.) derived from TimeOfDay.
 *
 * Usage:
 *   - Call update(dt) once per frame.
 *   - Query time() / sun() for rendering and lighting configuration.
 */

#pragma once

#include "TimeOfDay.h"
#include "Sun.h"

class Environment {
public:
    /*
     * Updates the environment simulation.
     *
     * Parameters:
     *   dt : Delta time in seconds. Expected to be non-negative.
     *
     * Side effects:
     *   - Advances internal TimeOfDay.
     *   - Recomputes Sun state based on the updated time-of-day.
     */
    void update(float dt);

    // Read-only access to the current time-of-day simulation state.
    const TimeOfDay& time() const { return m_time; }

    // Read-only access to the current sun state derived from time-of-day.
    const Sun& sun() const { return m_sun; }

private:
    TimeOfDay m_time;
    Sun m_sun;
};
