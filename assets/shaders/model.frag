#version 330 core
in vec3 vWorldPos;
in vec2 vUV;

uniform sampler2D uAlbedo;
uniform int uHasAlbedo;               // 0/1
uniform vec4 uBaseColorFactor;        // fallback * multiply

uniform vec4 uClipPlane;

out vec4 FragColor;

void main() {
    // clip: dot(vec4(worldPos,1), plane) < 0 => discard
    if (dot(vec4(vWorldPos, 1.0), uClipPlane) < 0.0) discard;

    vec4 c = uBaseColorFactor;
    if (uHasAlbedo == 1) c *= texture(uAlbedo, vUV);

    FragColor = vec4(c.rgb, 1.0);
}
