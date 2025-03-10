#version 330 core

layout (location = 0) in ivec3 aPos;
layout (location = 1) in ivec3 aColor;

out vec3 ourColor;

void main() {
    gl_Position = vec4(aPos.x / 320.0f - 1.0f, aPos.y / 240.0f - 1.0f, 0.0f, 1.0f);
    ourColor = vec3(aColor) / 255.0f;
}

