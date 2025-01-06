#ifndef EXCEPTIONS_ADDRESS_OUT_OF_BOUNDS_H
#define EXCEPTIONS_ADDRESS_OUT_OF_BOUNDS_H

#include <exception>

namespace exceptions {
class AddressOutOfBounds : public std::runtime_error {
public:
    explicit AddressOutOfBounds(const std::string &what)
        : std::runtime_error(what) {}
    explicit AddressOutOfBounds(const char *what)
        : std::runtime_error(what) {}
};
}

#endif
