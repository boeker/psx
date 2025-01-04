#ifndef EXCEPTIONS_UNIMPLEMENTED_INSTRUCTION_H
#define EXCEPTIONS_UNIMPLEMENTED_INSTRUCTION_H

#include <exception>

namespace exceptions {
class UnimplementedInstruction : public std::exception {
};
}

#endif
