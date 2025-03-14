#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;

out vec3 ourColor;

uniform ivec2 drawingAreaTopLeft;
uniform ivec2 drawingAreaBottomRight;

void main() {
    int width = drawingAreaBottomRight.x - drawingAreaTopLeft.x;
    int height = drawingAreaBottomRight.y - drawingAreaTopLeft.y;
    gl_Position = vec4(aPos.x / (0.5f * width) - 1.0f, aPos.y / (0.5f * height) - 1.0f, 0.0f, 1.0f);
    ourColor = vec3(aColor) / 255.0f;
}

