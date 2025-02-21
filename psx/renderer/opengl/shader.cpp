#include "shader.h"

#include <glad/glad.h>

namespace PSX {

Shader::Shader(const char *vertexPath, const char *fragmentPath) {
    // read shader sources from file
    // vertex shader
    std::string vertexSource;
    std::ifstream vertexSourceFile;
    vertexSourceFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    try {
        vertexSourceFile.open(vertexPath);
        std::stringstream vertexSourceStream;
        vertexSourceStream <<  vertexSourceFile.rdbuf();
        vertexSourceFile.close();
        vertexSource = vertexSourceStream.str();
    } catch (std::ifstream::failure &e) {
        std::cerr << "ERROR::SHADER::VERTEX::FILE_NOT_SUCCESSFULLY_READ" << std::endl;
    }

    const char *vertexSourceC = vertexSource.c_str();

    // fragment shader
    std::string fragmentSource;
    std::ifstream fragmentSourceFile;
    fragmentSourceFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    try {
        fragmentSourceFile.open(fragmentPath);
        std::stringstream fragmentSourceStream;
        fragmentSourceStream <<  fragmentSourceFile.rdbuf();
        fragmentSourceFile.close();
        fragmentSource = fragmentSourceStream.str();
    } catch (std::ifstream::failure &e) {
        std::cerr << "ERROR::SHADER::FRAGMENT::FILE_NOT_SUCCESSFULLY_READ" << std::endl;
    }

    const char *fragmentSourceC = fragmentSource.c_str();

    // compile shaders
    // vertex shader
    unsigned int vertexShader;
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSourceC, nullptr);
    glCompileShader(vertexShader);

    int success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED"
                  << std::endl << infoLog << std::endl;
    }

    // fragment shader
    unsigned int fragmentShader;
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSourceC, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED"
                  << std::endl << infoLog << std::endl;
    }

    // link shader objects into shader program object
    programID = glCreateProgram();
    glAttachShader(programID, vertexShader);
    glAttachShader(programID, fragmentShader);
    glLinkProgram(programID);

    glGetProgramiv(programID, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(programID, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED"
                  << std::endl << infoLog << std::endl;
    }

    // delete shader objects
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void Shader::use() {
    glUseProgram(programID);
}

void Shader::setBool(const std::string &name, bool value) const {
    glUniform1i(glGetUniformLocation(programID, name.c_str()), (int)value);
}

void Shader::setInt(const std::string &name, int value) const {
    glUniform1i(glGetUniformLocation(programID, name.c_str()), value);
}

void Shader::setFloat(const std::string &name, float value) const {
    glUniform1f(glGetUniformLocation(programID, name.c_str()), value);
}

//void Shader::setMat4(const std::string &name, const glm::mat4 &value) const {
//    glUniformMatrix4fv(glGetUniformLocation(programID, name.c_str()), 1, GL_FALSE, glm::value_ptr(value));
//}
//
//void Shader::setVec3v(const std::string &name, const glm::vec3 &value) const {
//    glUniform3fv(glGetUniformLocation(programID, name.c_str()), 1, glm::value_ptr(value));
//}

void Shader::setVec3(const std::string &name, float x, float y, float z) const {
    glUniform3f(glGetUniformLocation(programID, name.c_str()), x, y, z);
}

}

