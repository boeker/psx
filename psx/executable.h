#ifndef PSX_EXECUTABLE_H
#define PSX_EXECUTABLE_H

#include <cstdint>
#include <string>

namespace PSX {

class Executable {
private:
    uint8_t* exe;
    uint32_t exeLength;

public:
    Executable();
    virtual ~Executable();
    void reset();

    void readFromFile(const std::string &file);
    void writeExecutableToMemory();
};
}

#endif
