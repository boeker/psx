#ifndef PSX_GIO_H
#define PSX_GIO_H

#include <cstdint>
#include <iostream>
#include <string>

namespace PSX {

#define JOY_STAT_BAUDRATE_TIMER20 31
#define JOY_STAT_BAUDRATE_TIMER0 11
#define JOY_STAT_IRQ 9
#define JOY_STAT_ACK_INPUT_LEVEL 7
#define JOY_STAT_RX_PARITY_ERROR 3
#define JOY_STAT_TX_READY_FINISHED 2
#define JOY_STAT_RX_QUEUE_NOT_EMPTY 1
#define JOY_STAT_TX_READY_STARTED 0

#define JOY_MODE_CLK_OUTPUT_PARITY 8
#define JOY_MODE_PARITY_TYPE 5
#define JOY_MODE_PARITY_ENABLE 4
#define JOY_MODE_CHARACTER_LENGTH1 3
#define JOY_MODE_CHARACTER_LENGTH0 2
#define JOY_MODE_BAUDRATE_RELOAD_FACTOR1 1
#define JOY_MODE_BAUDRATE_RELOAD_FACTOR0 0

#define JOY_CTRL_SLOT_NUMBER 13
#define JOY_CTRL_ACK_INT_ENABLE 12
#define JOY_CTRL_RX_INT_ENABLE 11
#define JOY_CTRL_TX_INT_ENABLE 10
#define JOY_CTRL_RX_INT_MODE1 9
#define JOY_CTRL_RX_INT_MODE0 8
#define JOY_CTRL_RESET 6
#define JOY_CTRL_ACK 4
#define JOY_CTRL_RXEN 2
#define JOY_CTRL_JOYN_OUTPUT 1
#define JOY_CTRL_TXEN 0

class ReceiveQueue {
private:
    uint8_t queue[8];
    uint8_t in;
    uint8_t out;
    uint8_t elements;

    friend std::ostream& operator<<(std::ostream &os, const ReceiveQueue &queue);

public:
    ReceiveQueue();
    void clear();
    void push(uint8_t byte);
    uint8_t pop();
    bool isEmpty();
    bool isFull();
};

class Bus;
class Gamepad;

class GamepadMemcardIO {
private:
    Bus *bus;
    Gamepad &gamepad;

    // 0x1F801040
    // JOY_RX_DATA/JOY_TX_DATA

    // JOY_STAT
    // 0x1F801044
    uint32_t joyStat;

    // JOY_MODE
    // 0x1F801048
    uint16_t joyMode;

    // JOY_CTRL
    // 0x1F80104A
    uint16_t joyCtrl;

    // JOY_BAUD
    // 0x1F80104E
    uint16_t joyBaud;

    ReceiveQueue receiveQueue;

    uint8_t selectedSlot;
    uint8_t pendingTransferByte;
    bool pendingTransfer;

    uint32_t cyclesUntilInterrupt;

    friend std::ostream& operator<<(std::ostream &os, const GamepadMemcardIO &gmIO);

public:
    GamepadMemcardIO(Bus *bus, Gamepad &gamepad);
    void reset();

    template <typename T>
    void write(uint32_t address, T value);

    template <typename T>
    T read(uint32_t address);

    void writeToJoyCtrl(uint16_t value);

    void checkAndTransferPendingByte();
    void catchUpToCPU(uint32_t cyclesTaken);

private:

    std::string getJoyStatExplanation() const;
    std::string getJoyModeExplanation() const;
    std::string getJoyCtrlExplanation() const;
};

}

#endif

