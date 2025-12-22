#include "Environment.h"

void Environment::update(float dt) {
    m_time.update(dt);
    m_sun.update(m_time);
}
