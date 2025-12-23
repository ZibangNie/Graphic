/*
* LightingSystem.h
 *
 * Purpose:
 *   Declares LightingSystem, a small utility responsible for binding lighting-related uniforms
 *   to Shader programs using a consistent naming convention across the renderer.
 *
 * Responsibilities:
 *   - Apply an explicit DirectionalLight to a shader (sun direction/color/intensity + default ambient).
 *   - Apply lighting derived from the current Environment (sun + time-of-day driven ambient + camera position).
 *
 * Dependencies:
 *   - DirectionalLight is defined in environment/Sun.h.
 *   - Camera and Environment are forward-declared here to avoid circular includes.
 *
 * Uniform naming convention (expected by shaders):
 *   - uSunDir, uSunColor, uSunIntensity
 *   - uAmbientColor, uAmbientIntensity
 *   - uCameraPos
 *   - uTimeOfDay01 (optional; only meaningful if the shader declares it)
 */

#pragma once

#include "Shader.h"
#include "environment/Sun.h"   // Provides DirectionalLight.

// Forward declarations to avoid header inclusion cycles.
class Camera;
class Environment;

class LightingSystem {
public:
    /*
     * Applies a basic directional light to a shader.
     *
     * Parameters:
     *   shader : Target shader program wrapper.
     *   light  : Directional light parameters (direction, color, intensity).
     */
    void applyDirectionalLight(Shader& shader, const DirectionalLight& light) const;

    /*
     * Applies lighting parameters derived from the Environment to a shader.
     *
     * Parameters:
     *   shader : Target shader program wrapper.
     *   camera : Active camera; camera position is provided to the shader for view-dependent shading.
     *   env    : Environment providing sun (DirectionalLight) and normalized time-of-day.
     */
    void applyFromEnvironment(Shader& shader, const Camera& camera, const Environment& env) const;
};
