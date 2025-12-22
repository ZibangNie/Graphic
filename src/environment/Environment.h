#pragma once

#include "TimeOfDay.h"
#include "Sun.h"

class Environment {
public:
    void update(float dt);

    const TimeOfDay& time() const { return m_time; }
    const Sun& sun() const { return m_sun; }

private:
    TimeOfDay m_time;
    Sun m_sun;
};
