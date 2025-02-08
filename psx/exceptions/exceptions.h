#ifndef EXCEPTIONS_EXCEPTIONS_H
#define EXCEPTIONS_EXCEPTIONS_H

#include <exception>

namespace exceptions {

class AddressOutOfBounds : public std::runtime_error {
public:
    explicit AddressOutOfBounds(const std::string &what)
        : std::runtime_error(what) {}
    explicit AddressOutOfBounds(const char *what)
        : std::runtime_error(what) {}
};

class UnimplementedAddressingError : public std::runtime_error {
public:
    explicit UnimplementedAddressingError(const std::string &what)
        : std::runtime_error(what) {}
    explicit UnimplementedAddressingError(const char *what)
        : std::runtime_error(what) {}
};

class ExceptionNotImplemented : public std::runtime_error {
public:
    explicit ExceptionNotImplemented(const std::string &what)
        : std::runtime_error(what) {}
    explicit ExceptionNotImplemented(const char *what)
        : std::runtime_error(what) {}
};

class FileReadError : public std::runtime_error {
public:
    explicit FileReadError(const std::string &what)
        : std::runtime_error(what) {}
    explicit FileReadError(const char *what)
        : std::runtime_error(what) {}
};

class UnknownOpcodeError : public std::runtime_error {
public:
    explicit UnknownOpcodeError(const std::string &what)
        : std::runtime_error(what) {}
    explicit UnknownOpcodeError(const char *what)
        : std::runtime_error(what) {}
};

class UnknownFunctionError : public std::runtime_error {
public:
    explicit UnknownFunctionError(const std::string &what)
        : std::runtime_error(what) {}
    explicit UnknownFunctionError(const char *what)
        : std::runtime_error(what) {}
};

class UnknownGPUCommandError : public std::runtime_error {
public:
    explicit UnknownGPUCommandError(const std::string &what)
        : std::runtime_error(what) {}
    explicit UnknownGPUCommandError(const char *what)
        : std::runtime_error(what) {}
};

class UnknownDMACommandError : public std::runtime_error {
public:
    explicit UnknownDMACommandError(const std::string &what)
        : std::runtime_error(what) {}
    explicit UnknownDMACommandError(const char *what)
        : std::runtime_error(what) {}
};

}

#endif
