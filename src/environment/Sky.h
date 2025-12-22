#pragma once
#include "scene/Camera.h"
#include "Environment.h"

class Sky {
public:
    void render(const Camera& camera,
                const Environment& environment);
};
