#ifndef PSX_EXECUTABLE_H
#define PSX_EXECUTABLE_H

#include <cstdint>
#include <string>

namespace PSX {

class Bus;

struct ExecutableHeader {
    char asciiID[9];
    uint32_t initialPC;
    uint32_t initialR28;
    uint32_t destination;
    uint32_t fileSize;
    uint32_t memfillStart;
    uint32_t memfillSize;
    uint32_t initialR2930Base;
    uint32_t initialR2930Offset;
    std::string asciiMarker;
};

class Executable {
private:
    Bus *bus;

    uint8_t* exe;
    uint32_t exeLength;
    ExecutableHeader header;

public:
    Executable(Bus *bus);
    virtual ~Executable();
    void reset();

    void readFromFile(const std::string &file);
    bool loaded();
    void parseHeader();
    void writeToMemory();
};
}

#endif
