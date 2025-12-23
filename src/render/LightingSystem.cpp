/*
 * LightingSystem.cpp
 *
 * Purpose:
 *   Implements a lightweight lighting uniform binding layer.
 *   Centralizes how directional (sun) light, ambient light, and camera-dependent values are applied to shaders,
 *   ensuring consistent naming and conventions across multiple render passes (terrain, basic, water, particles, etc.).
 *
 * Conventions:
 *   - uSunDir is treated as the world-space direction toward the sun (used as L in N·L).
 *   - uSunColor and uSunIntensity define the directional light radiance multiplier.
 *   - uAmbientColor and uAmbientIntensity define a simple constant ambient term.
 *   - uCameraPos is provided for view-dependent shading (specular/Fresnel).
 *
 * Notes:
 *   - applyFromEnvironment() derives both sun and ambient terms from Environment (TimeOfDay + Sun).
 *   - Ambient intensity is intentionally low at night to avoid an overly bright scene.
 */

#include "LightingSystem.h"

#include "scene/Camera.h"
#include "environment/Environment.h"

#include <glm/glm.hpp>

/*
 * Applies an explicit directional light to a shader using the standard uniform names.
 *
 * Parameters:
 *   shader : Target shader program wrapper.
 *   light  : Directional light parameters (direction, color, intensity).
 *
 * Behavior:
 *   - Binds the shader and sets uSunDir/uSunColor/uSunIntensity.
 *   - Sets a default ambient term to avoid fully black unlit regions.
 *
 * Notes:
 *   - uSunDir is normalized before upload.
 *   - This function uses a fixed ambient intensity (0.35) and is intended for simple setups.
 */
void LightingSystem::applyDirectionalLight(Shader& shader, const DirectionalLight& light) const {
    shader.use();
    shader.setVec3("uSunDir", glm::normalize(light.direction));
    shader.setVec3("uSunColor", light.color);
    shader.setFloat("uSunIntensity", light.intensity);

    // Default ambient term (constant) to prevent complete blackness.
    shader.setVec3("uAmbientColor", glm::vec3(1.0f));
    shader.setFloat("uAmbientIntensity", 0.35f);
}

/*
 * Applies lighting parameters derived from the current Environment to a shader.
 *
 * Parameters:
 *   shader : Target shader program wrapper.
 *   camera : Active camera (camera.position is uploaded to uCameraPos).
 *   env    : Environment providing sun (DirectionalLight) and time-of-day.
 *
 * Behavior:
 *   - Uploads sun direction/color/intensity from env.sun().light().
 *   - Derives a "day factor" from sun elevation (sunY) and uses it to scale ambient intensity:
 *       ambIntensity = 0.01 + 0.20 * day^2
 *     yielding very low ambient at night and moderate ambient during the day.
 *   - Uploads camera position for view-dependent shading.
 *   - Uploads normalized time-of-day to uTimeOfDay01 (if the shader declares it).
 *
 * Notes:
 *   - Day factor is clamped to [0,1] and squared to increase contrast between day and night.
 *   - The uniform naming must match shader expectations (uSunDir, uSunColor, uSunIntensity, uAmbientColor, uAmbientIntensity, uCameraPos).
 *   - If a shader does not declare uTimeOfDay01, setting it should be a no-op in the Shader wrapper or ignored by GL.
 */
void LightingSystem::applyFromEnvironment(Shader& shader, const Camera& camera, const Environment& env) const {
    const DirectionalLight& L = env.sun().light();

    shader.use();
    shader.setVec3("uSunDir", glm::normalize(L.direction));
    shader.setVec3("uSunColor", L.color);
    shader.setFloat("uSunIntensity", L.intensity);

    // Compute day factor from sun elevation (Y component of normalized direction).
    float sunY = glm::normalize(L.direction).y;
    float day = glm::clamp((sunY - 0.02f) / 0.35f, 0.0f, 1.0f);
    day = day * day;

    // Ambient intensity: low at night; modest during day to avoid a uniformly bright scene.
    float ambIntensity = 0.01f + 0.20f * day;

    shader.setVec3("uAmbientColor", glm::vec3(1.0f));
    shader.setFloat("uAmbientIntensity", ambIntensity);

    // Camera world position for view-dependent terms (specular/Fresnel).
    shader.setVec3("uCameraPos", camera.position);

    // Normalized time-of-day. Only relevant if the target shader declares uTimeOfDay01.
    shader.setFloat("uTimeOfDay01", env.time().normalizedTime());
}
