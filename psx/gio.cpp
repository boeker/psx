#include "gio.h"

#include <cassert>
#include <cstring>
#include <format>
#include <sstream>

#include "bus.h"
#include "gamepad.h"
#include "util/bit.h"
#include "util/log.h"
#include "exceptions/exceptions.h"

using namespace util;

namespace PSX {

std::ostream& operator<<(std::ostream &os, const ReceiveQueue &queue) {
    ReceiveQueue copy = queue;

    if (!copy.isEmpty()) {
        os << std::format("0x{:08X}", copy.pop());
    }

    while (!copy.isEmpty()) {
        os << std::format(", 0x{:08X}", copy.pop());
    }

    return os;
}

ReceiveQueue::ReceiveQueue() {
    clear();
}

void ReceiveQueue::clear() {
    for (int i = 0; i < 8; ++i) {
        queue[i] = 0;
    }

    in = 0;
    out = 0;
    elements = 0;
}

void ReceiveQueue::push(uint8_t byte) {
    if (elements < 8) {
        queue[in] = byte;

        in = (in + 1) % 8;
        ++elements;
    }
}

uint8_t ReceiveQueue::pop() {
    if (elements > 0) {
        uint8_t value = queue[out];

        out = (out + 1) % 8;
        --elements;
        return value;
    }

    throw std::runtime_error("Queue is empty");

    return 0;
}

bool ReceiveQueue::isEmpty() {
    return elements == 0;
}

bool ReceiveQueue::isFull() {
    return elements == 8;
}

std::ostream& operator<<(std::ostream &os, const GamepadMemcardIO &gmIO) {
    os << "JOY_STAT: ";
    os << gmIO.getJoyStatExplanation();
    os << std::endl;
    os << "JOY_MODE: ";
    os << gmIO.getJoyModeExplanation();
    os << std::endl;
    os << "JOY_CTRL: ";
    os << gmIO.getJoyCtrlExplanation();
    os << std::endl;
    os << std::format("JOY_BAUD: 0x{:04X}", gmIO.joyBaud);

    return os;
}

GamepadMemcardIO::GamepadMemcardIO(Bus *bus, Gamepad &gamepad)
    : bus(bus), gamepad(gamepad) {

    reset();
}

void GamepadMemcardIO::reset() {
    joyStat = 0;
    joyMode = 0;
    joyCtrl = 0;
    joyBaud = 0;

    receiveQueue.clear();
}

template <>
void GamepadMemcardIO::write(uint32_t address, uint32_t value) {
    assert ((address >= 0x1F801040) && (address <= 0x1F80104F));

    LOGT_GIO(std::format("Write 0x{:08X} -> @0x{:08X}", value, address));

    if (address == 0x1F801040) {
        LOGV_GIO(std::format("Unimplemented write to JOY_TX_DATA of 0x{:08X}", value));

    } else if (address == 0x1F801044) {
        LOG_GIO(std::format("Attempted write to JOY_STAT of 0x{:08X}", value));
        // JOYT_STAT is read-only, ignore write
        return;

    } else {
        throw exceptions::UnimplementedAddressingError(std::format("GamepadMemcardIO: word write @0x{:08X}", address));
    }
}

template <> void GamepadMemcardIO::write(uint32_t address, uint16_t value) {
    assert ((address >= 0x1F801040) && (address <= 0x1F80104F));

    LOGT_GIO(std::format("Write 0x{:04X} -> @0x{:08X}", value, address));

    if (address == 0x1F801048) {
        LOGV_GIO(std::format("Half-word write to JOY_MODE of 0x{:04X}", value));
        joyMode = value & 0x013F; // bits 15-9, 7, and 6 are always zero
        LOGT_GIO(std::format("JOY_MODE: {:s}", getJoyModeExplanation()));

    } else if (address == 0x1F80104A) {
        writeToJoyCtrl(value);

    } else if (address == 0x1F80104E) {
        LOGT_GIO(std::format("Half-word write to JOY_BAUD of 0x{:04X}", value));
        joyBaud = value;

    } else {
        throw exceptions::UnimplementedAddressingError(std::format("GamepadMemcardIO: half-word write @0x{:08X}", address));
    }
}

template <> void GamepadMemcardIO::write(uint32_t address, uint8_t value) {
    assert ((address >= 0x1F801040) && (address <= 0x1F80104F));

    LOGT_GIO(std::format("Write 0x{:02X} -> @0x{:08X}", value, address));

    if (address == 0x1F801040) {
        LOGT_GIO(std::format("Write to JOY_TX_DATA of 0x{:02X}", value));
        uint8_t answer = gamepad.send(value);
        if (!receiveQueue.isFull()) {
            receiveQueue.push(answer);
        }

    } else {
        throw exceptions::UnimplementedAddressingError(std::format("GamepadMemcardIO: byte write @0x{:08X}", address));
    }
}

template <>
uint32_t GamepadMemcardIO::read(uint32_t address) {
    assert ((address >= 0x1F801040) && (address <= 0x1F80104F));

    LOGT_GIO(std::format("word read @0x{:08X}", address));

    if (address == 0x1F801040) {
        LOGV_GIO(std::format("Unimplemented read from JOY_RX_DATA"));
        return 0;

    } else if (address == 0x1F801044) {
        LOGV_GIO(std::format("Read from JOY_STAT"));
        LOGT_GIO(std::format("JOY_STAT: {:s}", getJoyStatExplanation()));
        return joyStat;

    } else {
        throw exceptions::UnimplementedAddressingError(std::format("GamepadMemcardIO: word read @0x{:08X}", address));
    }
}

template <> uint16_t GamepadMemcardIO::read(uint32_t address) {
    assert ((address >= 0x1F801040) && (address <= 0x1F80104F));

    LOGT_GIO(std::format("half-word read @0x{:08X}", address));

    if (address == 0x1F801044) {
        LOGT_GIO(std::format("Half-word read from JOY_STAT"));
        LOGT_GIO(std::format("JOY_STAT: {:s}", getJoyStatExplanation()));
        // return joyStat & 0x0000FFFF;
        LOGT_GIO(std::format("Returning 7 instead"));
        return 7;

    } else if (address == 0x1F801048) {
        LOGT_GIO(std::format("Half-word read from JOY_MODE"));
        LOGT_GIO(std::format("JOY_MODE: {:s}", getJoyModeExplanation()));
        return joyMode;

    } else if (address == 0x1F80104A) {
        LOGT_GIO(std::format("Half-word read from JOY_CTRL"));
        LOGT_GIO(std::format("JOY_CTRL: {:s}", getJoyCtrlExplanation()));
        return joyCtrl;

    } else if (address == 0x1F80104E) {
        LOGV_GIO(std::format("Half-word read from JOY_BAUD"));
        return joyBaud;

    } else {
        throw exceptions::UnimplementedAddressingError(std::format("GamepadMemcardIO: half-word read @0x{:08X}", address));
    }
}

template <> uint8_t GamepadMemcardIO::read(uint32_t address) {
    assert ((address >= 0x1F801040) && (address <= 0x1F80104F));

    LOGT_GIO(std::format("byte read @0x{:08X}", address));

    if (address == 0x1F801040) {
        if (!receiveQueue.isEmpty()) {
            uint8_t answer = receiveQueue.pop();
            LOGT_GIO(std::format("Byte read from JOY_RX_DATA, return 0x{:02X} from queue", answer));
            return answer;

        } else {
            LOGT_GIO(std::format("Byte read from JOY_RX_DATA, return 0x00 since queue is empty"));
            return 0x00;
        }

    } else {
        throw exceptions::UnimplementedAddressingError(std::format("GamepadMemcardIO: byte read @0x{:08X}", address));
    }
}

void GamepadMemcardIO::writeToJoyCtrl(uint16_t value) {
    LOGT_GIO(std::format("Write of 0x{:04X} to JOY_CTRL", value));

    // Clear bits 15, 14, 7 as these are always zero
    joyCtrl = value & 0x3F7F;
    LOGT_GIO(std::format("JOY_CTRL: {:s}", getJoyCtrlExplanation()));

    if (Bit::getBit(joyCtrl, JOY_CTRL_RESET)) { // Reset most (which?) JOY_* register to zero
        LOGT_GIO("JOY_CTRL: RESET bit set");
        Bit::clearBit(joyCtrl, JOY_CTRL_RESET);
        LOGT_GIO(std::format("JOY_CTRL: {:s}", getJoyCtrlExplanation()));

        joyStat = 0;
        joyMode = 0;
        joyBaud = 0;
    }

    if (Bit::getBit(joyCtrl, JOY_CTRL_ACK)) { // Reset interrupt request and priority error
        LOGT_GIO("JOY_CTRL: ACK bit set");
        Bit::clearBit(joyCtrl, JOY_CTRL_ACK);
        LOGT_GIO(std::format("JOY_CTRL: {:s}", getJoyCtrlExplanation()));

        Bit::clearBit(joyStat, JOY_STAT_IRQ);
        Bit::clearBit(joyStat, JOY_STAT_RX_PARITY_ERROR);
    }

}

void GamepadMemcardIO::transferByte(uint8_t value) {
    //TODO
}

std::string GamepadMemcardIO::getJoyStatExplanation() const {
    uint16_t stat = joyStat;

    std::stringstream ss;
    ss << std::format("BAUDRATE_TIMER[0x{:06X}], ", Bit::getBits<21>(stat, JOY_STAT_BAUDRATE_TIMER0));
    ss << std::format("IRQ[{:01b}], ", Bit::getBit(stat, JOY_STAT_IRQ));
    ss << std::format("ACK_INPUT_LEVEL[{:01b}], ", Bit::getBit(stat, JOY_STAT_ACK_INPUT_LEVEL));
    ss << std::format("RX_PARITY_ERROR[{:01b}], ", Bit::getBit(stat, JOY_STAT_RX_PARITY_ERROR));
    ss << std::format("TX_READY_FINISHED[{:01b}], ", Bit::getBit(stat, JOY_STAT_TX_READY_FINISHED));
    ss << std::format("RX_QUEUE_NOT_EMPTY[{:01b}], ", Bit::getBit(stat, JOY_STAT_RX_QUEUE_NOT_EMPTY));
    ss << std::format("TX_READY_STARTED[{:01b}], ", Bit::getBit(stat, JOY_STAT_TX_READY_STARTED));

    return ss.str();
}

std::string GamepadMemcardIO::getJoyModeExplanation() const {
    uint16_t mode = joyMode;

    std::stringstream ss;
    ss << std::format("CLK_OUT_PARITY[{:01b}], ", Bit::getBit(mode, JOY_MODE_CLK_OUTPUT_PARITY));
    ss << std::format("PARITY_TYPE[{:01b}], ", Bit::getBit(mode, JOY_MODE_PARITY_TYPE));
    ss << std::format("PARITY_ENABLE[{:01b}], ", Bit::getBit(mode, JOY_MODE_PARITY_ENABLE));
    ss << std::format("CHAR_LENGTH[{:d}], ", Bit::getBits<2>(mode, JOY_MODE_CHARACTER_LENGTH0));
    ss << std::format("BAUDRATE_FACTOR[{:d}]", Bit::getBits<2>(mode, JOY_MODE_BAUDRATE_RELOAD_FACTOR0));

    return ss.str();
}

std::string GamepadMemcardIO::getJoyCtrlExplanation() const {
    uint16_t ctrl = joyCtrl;

    std::stringstream ss;
    ss << std::format("SLOT_NUM[{:01b}], ", Bit::getBit(ctrl, JOY_CTRL_SLOT_NUMBER));
    ss << std::format("ACK_INT_ENABLE[{:01b}], ", Bit::getBit(ctrl, JOY_CTRL_ACK_INT_ENABLE));
    ss << std::format("RX_INT_ENABLE[{:01b}], ", Bit::getBit(ctrl, JOY_CTRL_RX_INT_ENABLE));
    ss << std::format("TX_INT_ENABLE[{:01b}], ", Bit::getBit(ctrl, JOY_CTRL_TX_INT_ENABLE));
    ss << std::format("RX_INT_MODE[{:d}], ", Bit::getBits<2>(ctrl, JOY_CTRL_RX_INT_MODE0));
    ss << std::format("RESET[{:01b}], ", Bit::getBit(ctrl, JOY_CTRL_RESET));
    ss << std::format("ACK[{:01b}], ", Bit::getBit(ctrl, JOY_CTRL_ACK));
    ss << std::format("RXEN[{:01b}], ", Bit::getBit(ctrl, JOY_CTRL_RXEN));
    ss << std::format("JOYN_OUTPUT[{:01b}], ", Bit::getBit(ctrl, JOY_CTRL_JOYN_OUTPUT));
    ss << std::format("TXEN[{:01b}]", Bit::getBit(ctrl, JOY_CTRL_TXEN));

    return ss.str();
}

}

