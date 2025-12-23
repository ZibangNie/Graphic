#version 330 core
in vec3 vColor;
in vec3 vWorldPos;     // NEW
out vec4 FragColor;

// ------- Phase A lighting uniforms (NEW) -------
uniform vec3  uSunDir;
uniform vec3  uSunColor;
uniform float uSunIntensity;

uniform vec3  uAmbientColor;
uniform float uAmbientIntensity;

uniform vec3  uCameraPos;
uniform vec3 uTint;

uniform int uEmissive; // 0/1

// ----------------------------------------------

void main() {
    if (uEmissive == 1) {
        FragColor = vec4(uTint, 1.0);
        return;
    }

    vec3 albedo = vColor;

    vec3 dpdx = dFdx(vWorldPos);
    vec3 dpdy = dFdy(vWorldPos);
    vec3 N = normalize(cross(dpdx, dpdy));

    vec3 L = normalize(uSunDir);
    float NdotL = max(dot(N, L), 0.0);

    vec3 ambient = albedo * uAmbientColor * uAmbientIntensity;
    vec3 diffuse = albedo * uSunColor * uSunIntensity * NdotL;

    vec3 color = ambient + diffuse;
    FragColor = vec4(color, 1.0);

}
