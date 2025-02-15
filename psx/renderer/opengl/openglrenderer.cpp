#include "openglrenderer.h"

#include <glad/glad.h>
#include <format>
#include <iostream>

#include "renderer/screen.h"
#include "util/log.h"

using namespace util;

namespace PSX {

const char *vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec3 aColor;\n"
    "out vec3 ourColor;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
    "   ourColor = aColor;\n"
    "}\0";
const char *fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "in vec3 ourColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = vec4(ourColor, 1.0f);\n"
    "}\n\0";

OpenGLRenderer::OpenGLRenderer(Screen *screen)
    : screen(screen) {
    // vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        LOG_WRN(std::format("Vertex shader compilation failed: {:s}", infoLog));
    }

    // fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        LOG_WRN(std::format("Fragment shader compilation failed: {:s}", infoLog));
    }

    // link shaders
    program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        LOG_WRN(std::format("Program linking failed: {:s}", infoLog));
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // vertex array object and vertex buffer object
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3* sizeof(float)));
    glEnableVertexAttribArray(1);
}

OpenGLRenderer::~OpenGLRenderer() {
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(program);
}

void OpenGLRenderer::drawTriangle(const Triangle &t) {
    std::cout << "drawTriangle()" << std::endl;
    float vertices[] =  {
        t.v1.x/320.0f - 1.0f, t.v1.y/240.0f - 1.0f, 0.0f, t.c1.r/255.0f, t.c1.g/255.0f, t.c1.b/255.0f,
        t.v2.x/320.0f - 1.0f, t.v2.y/240.0f - 1.0f, 0.0f, t.c2.r/255.0f, t.c2.g/255.0f, t.c2.b/255.0f,
        t.v3.x/320.0f - 1.0f, t.v3.y/240.0f - 1.0f, 0.0f, t.c3.r/255.0f, t.c3.g/255.0f, t.c3.b/255.0f
    };

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glUseProgram(program);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // unbind VBO and VAO
    //glBindBuffer(GL_ARRAY_BUFFER, 0);
    //glBindVertexArray(0);
}

void OpenGLRenderer::draw() {
}

void OpenGLRenderer::clear() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLRenderer::swapBuffers() {
    screen->swapBuffers();
}

}

