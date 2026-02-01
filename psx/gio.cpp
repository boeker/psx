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
    os << "JOY_BAUD: ";
    os << gmIO.getJoyBaudExplanation();

    return os;
}

GamepadMemcardIO::GamepadMemcardIO(Bus *bus, const Gamepad &gamepad)
    : bus(bus), gamepad(gamepad) {

    reset();
}

void GamepadMemcardIO::reset() {
    joyStat = 0;
    joyMode = 0;
    joyCtrl = 0;
    joyBaud = 0;
}

template <>
void GamepadMemcardIO::write(uint32_t address, uint32_t value) {
    assert ((address >= 0x1F801040) && (address <= 0x1F80104F));

    LOGT_GIO(std::format("write 0x{:08X} -> @0x{:08X}", value, address));

    if (address == 0x1F801040) {
        LOGV_GIO(std::format("Unimplemented write to JOY_TX_DATA of 0x{:08X}", value));

    } else if (address == 0x1F801044) {
        LOGV_GIO(std::format("Unimplemented write to JOY_STAT of 0x{:08X}", value));

    } else {
        throw exceptions::UnimplementedAddressingError(std::format("GamepadMemcardIO: word write @0x{:08X}", address));
    }
}

template <> void GamepadMemcardIO::write(uint32_t address, uint16_t value) {
    assert ((address >= 0x1F801040) && (address <= 0x1F80104F));

    LOGT_GIO(std::format("write 0x{:04X} -> @0x{:08X}", value, address));

    if (address == 0x1F801048) {
        LOGV_GIO(std::format("Unimplemented half-word write to JOY_MODE of 0x{:04X}", value));

    } else if (address == 0x1F80104A) {
        LOGV_GIO(std::format("Unimplemented half-word write to JOY_CTRL of 0x{:04X}", value));

    } else if (address == 0x1F80104E) {
        LOGT_GIO(std::format("Half-word write to JOY_BAUD of 0x{:04X}", value));
        joyBaud = value;

    } else {
        throw exceptions::UnimplementedAddressingError(std::format("GamepadMemcardIO: half-word write @0x{:08X}", address));
    }
}

template <> void GamepadMemcardIO::write(uint32_t address, uint8_t value) {
    assert ((address >= 0x1F801040) && (address <= 0x1F80104F));

    LOGT_GIO(std::format("write 0x{:02X} -> @0x{:08X}", value, address));

    if (address == 0x1F801040) {
        LOGV_GIO(std::format("Unimplemented write to JOY_TX_DATA of 0x{:02X}", value));

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
        LOGV_GIO(std::format("Unimplemented read from JOY_STAT"));
        return 0;

    } else {
        throw exceptions::UnimplementedAddressingError(std::format("GamepadMemcardIO: word read @0x{:08X}", address));
    }
}

template <> uint16_t GamepadMemcardIO::read(uint32_t address) {
    assert ((address >= 0x1F801040) && (address <= 0x1F80104F));

    LOGT_GIO(std::format("half-word read @0x{:08X}", address));

    if (address == 0x1F801044) {
        LOGV_GIO(std::format("Unimplemented half-word read from JOY_STAT"));
        //return 0;
        LOGV_GIO(std::format("Returning 7"));
        return 7;

    } else if (address == 0x1F801048) {
        LOGV_GIO(std::format("Unimplemented half-word read from JOY_MODE"));
        return 0;

    } else if (address == 0x1F80104A) {
        LOGV_GIO(std::format("Unimplemented half-word read from JOY_CTRL"));
        return 0;

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
        LOGV_GIO(std::format("Unimplemented byte read from JOY_RX_DATA"));
        return 0;

    } else {
        throw exceptions::UnimplementedAddressingError(std::format("GamepadMemcardIO: byte read @0x{:08X}", address));
    }
}

std::string GamepadMemcardIO::getJoyStatExplanation() const {
    //uint32_t dchr = dmaChannelControl[channel];

    //std::stringstream ss;
    //ss << std::format("START_TRIGGER[{:01b}], ",
    //                  (dchr >> DCHR_START_TRIGGER) & 1);
    //ss << std::format("START_BUSY[{:01b}], ",
    //                  (dchr >> DCHR_START_BUSY) & 1);
    //ss << std::format("CHOPPING_CPU_WIN_SIZE[{:01d}], ",
    //                  (dchr >> DCHR_CHOPPING_CPU_WINDOW_SIZE0) & 7);
    //ss << std::format("CHOPPING_DMA_WIN_SIZE[{:01d}], ",
    //                  (dchr >> DCHR_CHOPPING_DMA_WINDOW_SIZE0) & 7);
    //ss << std::format("SYNC_MODE[{:01d}], ",
    //                  (dchr >> DCHR_SYNC_MODE0) & 3);
    //ss << std::format("CHOPPING_ENABLE[{:01b}], ",
    //                  (dchr >> DCHR_CHOPPING_ENABLE) & 1);
    //ss << std::format("MEMORY_ADDRESS_STEP[{:01b}], ",
    //                  (dchr >> DCHR_MEMORY_ADDRESS_STEP) & 1);
    //ss << std::format("TRANSFER_DIRECTION[{:01b}]",
    //                  (dchr >> DCHR_TRANSFER_DIRECTION) & 1);

    //return ss.str();
    return "";
}

std::string GamepadMemcardIO::getJoyModeExplanation() const {
    return "";
}

std::string GamepadMemcardIO::getJoyCtrlExplanation() const {
    return "";
}

std::string GamepadMemcardIO::getJoyBaudExplanation() const {
    return "";
}

}

