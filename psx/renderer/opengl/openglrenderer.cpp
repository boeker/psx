#include "openglrenderer.h"

#include <glad/glad.h>
#include <cstring>
#include <format>
#include <iostream>

#include "renderer/screen.h"
#include "util/log.h"
#include "shader.h"
#include "gl.h"

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

    vram = new uint8_t[VRAM_SIZE];

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
    float vertices[] =  {
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3* sizeof(float)));
    glEnableVertexAttribArray(1);


    // store VRAM in a texture
    glGenFramebuffers(1, &vramFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, vramFramebuffer);

    glCheckError();

    // texture attachment
    glGenTextures(1, &vramTexture);
    glBindTexture(GL_TEXTURE_2D, vramTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1024, 512, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vramTexture, 0);

    glCheckError();

    // create a renderbuffer object for depth and stencil attachment (we won't be sampling these)
    //unsigned int rbo;
    //glGenRenderbuffers(1, &rbo);
    //glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    //glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1024, 512); // use a single renderbuffer object for both a depth AND stencil buffer.
    //glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo); // now actually attach it

    glCheckError();

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;



    // fill the vram texture with red for debugging purposes
    glViewport(0, 32, 640, 480);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);



    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glCheckError();

    // quad
    float quadVertices[] = { // vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glCheckError();

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glCheckError();

    screenShader = new Shader("shaders/screen.vs", "shaders/screen.fs");
    screenShader->use();
    screenShader->setInt("screenTexture", 0);

    glCheckError();
}

OpenGLRenderer::~OpenGLRenderer() {
    delete[] vram;

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(program);
}

void OpenGLRenderer::reset() {
    std::memset(vram, 0, VRAM_SIZE);
}

void OpenGLRenderer::clear() {
    //glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    //glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLRenderer::computeViewport() {
    // compute where to place vram framebuffer on screen
    int windowHeight = screen->getHeight();
    int windowWidth = screen->getWidth();

    int height = windowHeight;
    int width = (windowHeight / 3) * 4;
    if (width > windowWidth) {
        height = (windowWidth / 4) * 3;
        width = windowWidth;
    }

    viewportWidth = width;
    viewportHeight = height;
    viewportX = (windowWidth - width) / 2;
    viewportY = (windowHeight - height) / 2;
}

void OpenGLRenderer::swapBuffers() {
    glCheckError();

    // compute viewport coordinates from window size
    computeViewport();

    // set new viewport
    glViewport(viewportX, viewportY, viewportWidth, viewportHeight);

    //// render (part of) vram texture to screen (default framebuffer)
    //glBindFramebuffer(GL_FRAMEBUFFER, 0);
    //screenShader->use();
    //glBindVertexArray(quadVAO);
    //glBindTexture(GL_TEXTURE_2D, vramTexture);
    //glDrawArrays(GL_TRIANGLES, 0, 6);
    //glCheckError();


    // blit vram framebuffer to default framebuffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, vramFramebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, 1024, 512,
    //glBlitFramebuffer(0, 32, 640, 512,
                      //viewportX, viewportY, viewportX + viewportWidth, viewportY + viewportHeight,
                      viewportX, viewportY + viewportHeight, viewportX + viewportWidth, viewportY, // flip texture along y-axis
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glCheckError();

    // swap buffers
    screen->swapBuffers();

    glCheckError();
}

void OpenGLRenderer::drawTriangle(const Triangle &t) {
    glCheckError();

    glViewport(0, 0, 640, 480);
    glBindFramebuffer(GL_FRAMEBUFFER, vramFramebuffer);

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
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLRenderer::writeToVRAM(uint32_t line, uint32_t pos, uint16_t value) {
    //LOG_REND_VRAM(std::format("VRAM write 0x{:04X} -> line {:d}, position {:d}",
    //                          value, line, pos));

    uint16_t *vramLine = (uint16_t*)&(vram[512 * line]);
    vramLine[pos] = value;
}

uint16_t OpenGLRenderer::readFromVRAM(uint32_t line, uint32_t pos) {
    uint16_t *vramLine = (uint16_t*)&(vram[512 * line]);
    uint16_t value = vramLine[pos];

    //LOG_REND_VRAM(std::format("VRAM read line {:d}, position {:d} -> 0x{:04X}",
    //                          line, pos, value));

    return value;
}

void OpenGLRenderer::writeToVRAM(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint8_t *data) {
    LOG_REND_VRAM(std::format("VRAM write to {:d}, {:d} of size {:d}x{:d}",
                              x, y, width, height));

    glBindTexture(GL_TEXTURE_2D, vramTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, data);
}

}

