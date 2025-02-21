#ifndef SHADER_H
#define SHADER_H

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

//#include <glad/glad.h>
//#include <glm/glm.hpp>
//#include <glm/gtc/type_ptr.hpp>

namespace PSX {

class Shader {
private:
    // program ID
    unsigned int programID;

public:
    // constructor reads source and builds shader
    Shader(const char *vertexPath, const char *fragmentPath);

    // use/activate the shader
    void use();

    // functions for setting uniforms
    void setBool(const std::string &name, bool value) const;
    void setInt(const std::string &name, int value) const;
    void setFloat(const std::string &name, float value) const;
    //void setMat4(const std::string &name, const glm::mat4 &value) const;
    //void setVec3v(const std::string &name, const glm::vec3 &value) const;
    void setVec3(const std::string &name, float x, float y, float z) const;
};

}

#endif
