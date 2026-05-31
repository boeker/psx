#ifndef UTIL_QUEUE_H
#define UTIL_QUEUE_H

#include <cstdint>
#include <format>
#include <ostream>

namespace util {

template<unsigned int N>
class Queue {
private:
    uint8_t queue[N];
    uint8_t in;
    uint8_t out;
    uint8_t elements;

    template<unsigned int M>
    friend std::ostream& operator<<(std::ostream &os, const Queue<M> &queue);

public:
    Queue() {
        clear();
    }

    void clear() {
        for (int i = 0; i < N; ++i) {
            queue[i] = 0;
        }

        in = 0;
        out = 0;
        elements = 0;
    }

    void push(uint8_t parameter) {
        if (elements < N) {
            queue[in] = parameter;

            in = (in + 1) % N;
            ++elements;
        }
    }

    uint8_t pop() {
        if (elements > 0) {
            uint8_t value = queue[out];

            out = (out + 1) % N;
            --elements;
            return value;
        }

        throw std::runtime_error("Queue is empty");

        return 0;
    }

    bool is_empty() {
        return elements == 0;
    }

    bool is_full() {
        return elements == N;
    }
};

}

template<unsigned int N>
std::ostream& operator<<(std::ostream &os, const util::Queue<N> &queue) {
    util::Queue<N> copy = queue;

    if (!copy.isEmpty()) {
        os << std::format("0x{:08X}", copy.pop());
    }

    while (!copy.isEmpty()) {
        os << std::format(", 0x{:08X}", copy.pop());
    }

    return os;
}

#endif

