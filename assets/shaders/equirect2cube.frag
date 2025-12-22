#version 330 core
out vec4 FragColor;

in vec3 vLocalPos;

uniform sampler2D uEquirect;

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 sampleSphericalMap(vec3 v) {
    // v: normalized direction
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main() {
    vec3 dir = normalize(vLocalPos);
    vec2 uv = sampleSphericalMap(dir);

    // 常见 HDR 需要把 V 翻一下（如果你发现上下颠倒，把这行注释/反注释即可）
    uv.y = 1.0 - uv.y;

    vec3 color = texture(uEquirect, uv).rgb;
    FragColor = vec4(color, 1.0);
}
