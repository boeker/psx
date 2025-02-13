#include "glrender.h"

#include <glad/glad.h>
#include <format>
#include <iostream>

#include "psx/util/log.h"

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

GLRender::GLRender() {
    // vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        LOG_WAR(std::format("Vertex shader compilation failed: {:s}", infoLog));
    }

    // fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        LOG_WAR(std::format("Fragment shader compilation failed: {:s}", infoLog));
    }

    // link shaders
    program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        LOG_WAR(std::format("Program linking failed: {:s}", infoLog));
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

GLRender::~GLRender() {
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(program);
}

void GLRender::drawTriangle(const Triangle &triangle) {
    std::cout << "drawTriangle()" << std::endl;
    float vertices[] =  {
        triangle.x1/1024.0f, triangle.y1/1024.0f, 0.0f, triangle.r1/255.0f, triangle.g1/255.0f, triangle.b1/255.0f,
        triangle.x2/1024.0f, triangle.y2/1024.0f, 0.0f, triangle.r2/255.0f, triangle.g2/255.0f, triangle.b2/255.0f,
        triangle.x3/1024.0f, triangle.y3/1024.0f, 0.0f, triangle.r3/255.0f, triangle.g3/255.0f, triangle.b3/255.0f
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

void GLRender::draw() {
}

void GLRender::clear() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

}

