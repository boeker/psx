#include "cdrom.h"

#include <cassert>
#include <format>

#include "exceptions/exceptions.h"
#include "util/bit.h"
#include "util/log.h"

#include "bus.h"

using namespace util;

namespace PSX {

std::ostream& operator<<(std::ostream &os, const Queue &queue) {
    Queue copy = queue;

    if (!copy.isEmpty()) {
        os << std::format("0x{:08X}", copy.pop());
    }

    while (!copy.isEmpty()) {
        os << std::format(", 0x{:08X}", copy.pop());
    }

    return os;
}

Queue::Queue() {
    clear();
}

void Queue::clear() {
    for (int i = 0; i < 16; ++i) {
        queue[i] = 0;
    }

    in = 0;
    out = 0;
    elements = 0;
}

void Queue::push(uint8_t parameter) {
    if (elements < 16) {
        queue[in] = parameter;

        in = (in + 1) % 16;
        ++elements;
    }
}

uint8_t Queue::pop() {
    if (elements > 0) {
        uint8_t value = queue[out];

        out = (out + 1) % 16;
        --elements;
        return value;
    }

    throw std::runtime_error("Queue is empty");

    return 0;
}

bool Queue::isEmpty() {
    return elements == 0;
}

bool Queue::isFull() {
    return elements == 16;
}

CDROM::CDROM(Bus *bus)
    : bus(bus) {

    reset();
}

void CDROM::reset() {
    statusRegister = 0x18; // All queues are empty

    audioVolumeCDOutToSPUIn[0] = 0x80;
    audioVolumeCDOutToSPUIn[1] = 0x80;
    audioVolumeCDOutToSPUIn[2] = 0x80;
    audioVolumeCDOutToSPUIn[3] = 0x80;

    interruptEnableRegister = 0;
    interruptFlagRegister = 0;
    requestRegister = 0;

    command = 0;
    function = 0;
    parameterQueue.clear();
    responseInterrupt = 0;
    responseQueue.clear();
    secondResponseInterrupt = 0;
    secondResponseQueue.clear();
}

void CDROM::updateStatusRegister() {
    Bit::setBit(statusRegister, CDROM_STATUS_BUSYSTS, 0); // TODO Busy during command/parameter transmission
    Bit::setBit(statusRegister, CDROM_STATUS_DRQSTS, 0); // TODO Data queue
    Bit::setBit(statusRegister, CDROM_STATUS_RSLRRDY, !responseQueue.isEmpty());
    Bit::setBit(statusRegister, CDROM_STATUS_PRMWRDY, !parameterQueue.isFull());
    Bit::setBit(statusRegister, CDROM_STATUS_PRMEMPT, parameterQueue.isEmpty());
    Bit::setBit(statusRegister, CDROM_STATUS_ADPBUSY, 0); // TODO XA-ADPCM
}

uint8_t CDROM::getIndex() const {
    return statusRegister & 0x3;
}

void CDROM::executeCommand() {
    if (Bit::getBit(requestRegister, CDROM_REQUEST_SMEN)) {
        notifyAboutINT10();
    }

    // Execute command
    (this->*commands[command])();

    // Check if there is a response (commands never return INT8 or INT10)
    if (responseInterrupt != 0) {
        notifyAboutINT1to7(responseInterrupt);
    }
}

void CDROM::notifyAboutINT1to7(uint8_t interruptNumber) {
    assert(interruptNumber >= 1 && interruptNumber <= 7);
    LOGV_CDROM(std::format("Notifying about INT{:d}", interruptNumber));
    // Only trigger if there was no interrupt before. Is this correct?
    bool wasInterrupt = interruptFlagRegister & 0x7;
    interruptFlagRegister = interruptFlagRegister | interruptNumber;
    // Check if any of the three enable bits is enabled
    uint8_t interrupt = interruptEnableRegister & interruptFlagRegister & 0x7;
    if (!wasInterrupt && interrupt) {
        LOGV_CDROM(std::format("Issuing INT{:d}", interrupt));
        bus->interrupts.notifyAboutInterrupt(INTERRUPT_BIT_CDROM);
    }
}

void CDROM::notifyAboutINT10() {
    LOGV_CDROM("Notifying about and issuing INT10");
    // Only trigger if there was no interrupt before. Is this correct?
    bool wasInterrupt = Bit::getBit(interruptFlagRegister, CDROM_INTERRUPT_FLAG_CLRBFWRDY);
    Bit::setBit(interruptFlagRegister, CDROM_INTERRUPT_FLAG_CLRBFWRDY);
    if (!wasInterrupt && Bit::getBit(interruptEnableRegister, CDROM_INTERRUPT_ENABLE_BFWRDY)) {
        LOGV_CDROM("Issuing INT10");
        bus->interrupts.notifyAboutInterrupt(INTERRUPT_BIT_CDROM);
    }
}

void CDROM::updateInterruptFlagRegister(uint8_t value) {
    // 7 - CHPRST: Unknown

    // 6 - CLRPRM: Reset Parameter Queue
    if (Bit::getBit(value, CDROM_INTERRUPT_FLAG_CLRPRM)) {
        parameterQueue.clear();
    }

    // 5 - SMADPCLR: Unknown/Clear sound map out

    // 4 - CLRBFWRDY: Acknowledge INT10
    if (Bit::getBit(value, CDROM_INTERRUPT_FLAG_CLRBFWRDY)) {
        Bit::clearBit(interruptFlagRegister, CDROM_INTERRUPT_FLAG_CLRBFWRDY);
    }

    // 3 - CLRBFEMPT: Acknowledge INT8
    if (Bit::getBit(value, CDROM_INTERRUPT_FLAG_CLRBFEMPT)) {
        Bit::clearBit(interruptFlagRegister, CDROM_INTERRUPT_FLAG_CLRBFEMPT);
    }

    // 2 to 0: Acknowledge INT1...7
    bool wasInterrupt = interruptFlagRegister & 0x3;
    interruptFlagRegister = interruptFlagRegister & ~(value & 0x3);
    bool isInterrupt = interruptFlagRegister & 0x3;

    // Acknowledge empties response queue, sends pending command (if there is one)
    // But what counts as an "acknowledge"?
    // Would clearing a single bit of, say, INT3 suffice?
    // And what about INT10 and INT8?
    // Let's assume that we only consider INT1...7 and that it has to be cleared completely
    if (wasInterrupt && !isInterrupt) {
        // Clear response queue
        responseQueue.clear();

        // Check if there is a second response waiting
        if (secondResponseInterrupt != 0) {
            // swap first and second response queue
            std::swap(responseQueue, secondResponseQueue);
            notifyAboutINT1to7(secondResponseInterrupt);
        }
    }
}

template <>
void CDROM::write(uint32_t address, uint32_t value) {
    throw exceptions::UnimplementedAddressingError(std::format("word write @0x{:08X}", address));
}

template <>
void CDROM::write(uint32_t address, uint16_t value) {
    throw exceptions::UnimplementedAddressingError(std::format("half-word write @0x{:08X}", address));
}

template <>
void CDROM::write(uint32_t address, uint8_t value) {
    assert ((address >= 0x1F801800) && (address < 0x1F801804));

    LOGT_CDROM(std::format("Write of 0x{:02X} -> @0x{:08X} with index {:d}", value, address, getIndex()));

    if (address == 0x1F801800) { // status register
        LOG_CDROM(std::format("Write to status register: 0x{:02X}", value));
        // Only the index can be written to
        statusRegister = (statusRegister & 0xF8) | value & 0x3;

    } else if (address == 0x1F801801) {
        switch (getIndex()) {
            case 0: // Command Register
                LOGV_CDROM(std::format("Write to command register: 0x{:02X}", value));
                command = value;
                executeCommand();
                break;
            case 1: // Sound Map Data Out
                LOG_CDROM(std::format("Unimplemented write to Sound Map Data Out: 0x{:02X} -> @0x{:08X} with index {:d}", value, address, getIndex()));
                // TODO Implement
                break;
            case 2: // Sound Map Coding Info
                LOG_CDROM(std::format("Unimplemented write to Sound Map Coding Info: 0x{:02X} -> @0x{:08X} with index {:d}", value, address, getIndex()));
                // TODO Implement
                break;
            case 3: // Audio Volume for Right-CD-Out to Right-SPU-Input
                LOGV_CDROM(std::format("Write to Audio Volume for Right-CD-Out to Right-SPU-Input: 0x{:02X}", value));
                audioVolumeCDOutToSPUIn[3] = value;
                break;
            default:
                assert(false);
                break;
        }
    } else if (address == 0x1F801802) {
        switch (getIndex()) {
            case 0: // Parameter Queue
                LOGV_CDROM(std::format("Write to parameter queue: 0x{:02X}", value));
                parameterQueue.push(value);
                break;
            case 1: // Interrupt Enable Register
                LOGV_CDROM(std::format("Write to interrupt enable register: 0x{:02X}", value));
                interruptEnableRegister = value & 0x1F; // Bit 7 to 5 should be zero. Only use 4 to 0.
                // Should we check and issue pending interrupts here?
                break;
            case 2: // Audio Volume for Left-CD-Out to Left-SPU-Input
                LOGV_CDROM(std::format("Write to Audio Volume for Left-CD-Out to Left-SPU-Input: 0x{:02X}", value));
                audioVolumeCDOutToSPUIn[0] = value;
                break;
            case 3: // Audio Volume for Right-CD-Out to Left-SPU-Input
                LOGV_CDROM(std::format("Write to Audio Volume for Right-CD-Out to Left-SPU-Input: 0x{:02X}", value));
                audioVolumeCDOutToSPUIn[2] = value;
                break;
            default:
                assert(false);
                break;
        }
    } else if (address == 0x1F801803) {
        switch (getIndex()) {
            case 0: // Request Register
                LOG_CDROM(std::format("Unimplemented write to request register: 0x{:02X}", value));
                requestRegister = value;
                if (Bit::getBit(value, CDROM_REQUEST_BFRD)) {
                    // TODO Load data queue
                } else {
                    // TODO Clear data queue
                }
                break;
            case 1: // Interrupt Flag Register
                LOG_CDROM(std::format("Write to Interrupt Flag Register: 0x{:02X}", value));
                updateInterruptFlagRegister(value);
                break;
            case 2: // Audio Volume for Left-CD-Out to Right-SPU-Input
                LOGV_CDROM(std::format("Write to Audio Volume for Left-CD-Out to Right-SPU-Input: 0x{:02X}", value));
                audioVolumeCDOutToSPUIn[1] = value;
                break;
            case 3: // Audio Volume Apply Changes
                LOG_CDROM(std::format("Unimplemented write to Audio Volume Apply Changes: 0x{:02X}", value));
                // TODO Implement
                break;
            default:
                assert(false);
                break;
        }

    } else {
        LOG_CDROM(std::format("Unimplemented write 0x{:02X} -> @0x{:08X} with index {:d}", value, address, getIndex()));
    }
}

template <>
uint32_t CDROM::read(uint32_t address) {
    throw exceptions::UnimplementedAddressingError(std::format("word read @0x{:08X}", address));
}

template <>
uint16_t CDROM::read(uint32_t address) {
    throw exceptions::UnimplementedAddressingError(std::format("half-word read @0x{:08X}", address));
}

template <>
uint8_t CDROM::read(uint32_t address) {
    assert ((address >= 0x1F801800) && (address < 0x1F801804));

    uint8_t value = 0;

    if (address == 0x1F801800) { // status register
        updateStatusRegister();
        value = statusRegister;
        LOGV_CDROM(std::format("Read from status register: 0x{:02X}", value));

    } else if (address == 0x1F801801) {
        switch (getIndex()) {
            case 0: // Mirror of response queue
            case 1: // Response queue
            case 2: // Mirror of response queue
            case 3: // Mirror of response queue
                if (responseQueue.isEmpty()) {
                    LOG_CDROM(std::format("Response queue is empty!"));
                    value = 0;

                } else {
                    value = responseQueue.pop();
                }
                // TODO Implement wrap-around of response queue

                LOGV_CDROM(std::format("Reading response from response queue: 0x{:02X}", value));
                break;
            default:
                assert(false);
                break;
        }

    } else if (address == 0x1F801802) {
        switch (getIndex()) {
            case 0: // Data Queue
            case 1: // Mirror of data queue
            case 2: // Mirror of data queue
            case 3: // Mirror of data queue
                LOG_CDROM("Unimplemented read from data queue");
                // TODO Implement data queue
                //LOG_CDROM(std::format("Reading byte from data queue: 0x{:02X}", value));
                break;
            default:
                assert(false);
                break;
        }

    } else if (address == 0x1F801803) {
        switch (getIndex()) {
            case 0: // Interrupt Enabled Register
            case 2: // Mirror of Interrupt Enable Register
                value = interruptEnableRegister | 0xE0; // Bits 7 to 5 unused, usually 1 on read
                LOGV_CDROM(std::format("Read from interrupt enable register: 0x{:02X}", value));
                break;
            case 1: // Interrupt Flag Register
            case 3: // Mirror of Interrupt Flag Register
                value = interruptFlagRegister | 0xE0; // Bits 7 to 5 are always 1
                LOGV_CDROM(std::format("Read from interrupt flag register: 0x{:02X}", value));
                break;
            default:
                assert(false);
                break;
        }

    } else {
        LOG_CDROM(std::format("Unimplemented read @0x{:08X} with index {:d}", address, getIndex()));
    }

    LOGT_CDROM(std::format("Read @0x{:08X} -> 0x{:02X}", address, value));

    return value;
}

const CDROM::Command CDROM::commands[] = {
    // 0x00
    &CDROM::Unknown,
    &CDROM::Getstat, // 0x01
    &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    // 0x10
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown,
    &CDROM::Test, // 0x19
    &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    // 0x20
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    // 0x30
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    // 0x40
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    // 0x50
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    // 0x60
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    // 0x70
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    // 0x80
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    // 0x90
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    // 0xA0
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    // 0xB0
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    // 0xC0
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    // 0xD0
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    // 0xE0
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    // 0xF0
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown
};

const CDROM::Command CDROM::subFunctions[] = {
    // 0x00
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    // 0x10
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    // 0x20
    &CDROM::Function0x20,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    // 0x30
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    // 0x40
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    // 0x50
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    // 0x60
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    // 0x70
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    // 0x80
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    // 0x90
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    // 0xA0
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    // 0xB0
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    // 0xC0
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    // 0xD0
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    // 0xE0
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    // 0xF0
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF,
    &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF, &CDROM::UnknownSF
};

void CDROM::Unknown() {
    throw exceptions::UnknownCDROMCommandError(std::format("Unknown command 0x{:02X}", command));
}

void CDROM::Getstat() {
    LOG_CDROM(std::format("Command: Getstat"));
    //7  Play          Playing CD-DA         ;\only ONE of these bits can be set
    //6  Seek          Seeking               ; at a time (ie. Read/Play won't get
    //5  Read          Reading data sectors  ;/set until after Seek completion)
    //4  ShellOpen     Once shell open (0=Closed, 1=Is/was Open)
    //3  IdError       (0=Okay, 1=GetID denied) (also set when Setmode.Bit4=1)
    //2  SeekError     (0=Okay, 1=Seek error)     (followed by Error Byte)
    //1  Spindle Motor (0=Motor off, or in spin-up phase, 1=Motor on)
    //0  Error         Invalid Command/parameters (followed by Error Byte)

    responseInterrupt = 3;
    responseQueue.push(0); // Empty drive (?)
    //responseQueue.push(1 << 3); // GetID denied for now
    //responseQueue.push(1 << 4); // ShellOpen for now
}

void CDROM::Test() {
    LOG_CDROM(std::format("Command: Test"));
    function = parameterQueue.pop();

    // Execute sub-function
    (this->*subFunctions[function])();
}

void CDROM::GetID() {
    LOG_CDROM(std::format("Command: GetID"));
    // INT3 with status first, then INT5
    responseInterrupt = 3;
    responseQueue.push(0); // Status code, i.e., drive closed with no disc
    //responseQueue.push(0x11);
    //responseQueue.push(0x80);

    // hard-coded second answer
    secondResponseInterrupt = 5;
    secondResponseQueue.push(0x08);
    secondResponseQueue.push(0x40);
    secondResponseQueue.push(0x00);
    secondResponseQueue.push(0x00);
    //secondResponseQueue.push(0x00);
    //secondResponseQueue.push(0x00);
    //secondResponseQueue.push(0x00);
    //secondResponseQueue.push(0x00);
}

void CDROM::UnknownSF() {
    throw exceptions::UnknownCDROMFunctionError(std::format("Unknown function 0x{:02X}", function));
}

void CDROM::Function0x20() {
    // hard-coded answer
    responseInterrupt = 3;
    responseQueue.push(90); // Year
    responseQueue.push(1); // Month
    responseQueue.push(1); // Day
    responseQueue.push(1); // Version
}

}

