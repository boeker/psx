#ifndef EXCEPTIONS_MEMORY_H
#define EXCEPTIONS_MEMORY_H

#include <exception>
#include <stdexcept>
#include <string>

namespace exceptions {

class FileReadError : public std::runtime_error {
public:
    explicit FileReadError(const std::string &what);
    explicit FileReadError(const char *what);
};
}

#endif
