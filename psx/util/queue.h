#ifndef UTIL_QUEUE_H
#define UTIL_QUEUE_H

#include <cstdint>
#include <format>
#include <ostream>

namespace util {

template<typename T, unsigned int N>
class RingBuffer {
private:
    T queue[N];
    uint8_t in;
    uint8_t out;
    uint8_t elements;

    template<typename S, unsigned int M>
    friend std::ostream& operator<<(std::ostream &os, const RingBuffer<S, M> &queue);

public:
    RingBuffer() {
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

    bool push(T parameter) {
        if (elements < N) {
            queue[in] = parameter;

            in = (in + 1) % N;
            ++elements;
            return true;
        }

        return false;
    }

    T pop() {
        if (elements > 0) {
            T value = queue[out];

            out = (out + 1) % N;
            --elements;
            return value;
        }

        throw std::runtime_error("RingBuffer is empty");

        return 0;
    }

    bool is_empty() {
        return elements == 0;
    }

    bool is_full() {
        return elements == N;
    }

    uint8_t size() {
        return elements;
    }
};

template <unsigned int N>
using Queue = RingBuffer<uint8_t, N>;

}

template<typename T, unsigned int N>
std::ostream& operator<<(std::ostream &os, const util::RingBuffer<T, N> &queue) {
    util::RingBuffer<T, N> copy = queue;

    if (!copy.isEmpty()) {
        os << std::format("0x{:08X}", copy.pop());
    }

    while (!copy.isEmpty()) {
        os << std::format(", 0x{:08X}", copy.pop());
    }

    return os;
}

#endif

