/*
* Environment.cpp
 *
 * Purpose:
 *   Implements Environment orchestration logic.
 *   Advances time-of-day and updates dependent environmental systems (e.g., sun direction/color).
 *
 * Notes:
 *   - Environment acts as a coordinator between TimeOfDay and Sun so rendering systems can query a
 *     consistent environment state each frame.
 */

#include "Environment.h"

/*
 * Advances environment simulation by one frame.
 *
 * Parameters:
 *   dt : Delta time in seconds. Expected to be non-negative; typical frame values are in the range ~[0.0, 0.1].
 *
 * Side effects:
 *   - Advances the internal TimeOfDay state.
 *   - Updates the Sun state based on the updated time-of-day.
 */
void Environment::update(float dt) {
    m_time.update(dt);
    m_sun.update(m_time);
}
