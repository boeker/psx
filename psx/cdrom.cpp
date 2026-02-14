#include "cdrom.h"

#include <cassert>
#include <format>

#include "exceptions/exceptions.h"
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

    audioVolumeLeftCDOutToLeftSPUInput = 0x80;
    audioVolumeLeftCDOutToRightSPUInput = 0x80;
    audioVolumeRightCDOutToLeftSPUInput = 0x80;
    audioVolumeRightCDOutToRightSPUInput = 0x80;

    interruptEnableRegister = 0;
    interruptFlagRegister = 0;

    parameterQueue.clear();
    responseQueue.clear();
}

uint8_t CDROM::getIndex() const {
    return statusRegister & 0x3;
}

void CDROM::executeCommand(uint8_t command) {
    if (command == 0x01) { // Getstat
        LOG_CDROM(std::format("Command: Getstat"));
        //7  Play          Playing CD-DA         ;\only ONE of these bits can be set
        //6  Seek          Seeking               ; at a time (ie. Read/Play won't get
        //5  Read          Reading data sectors  ;/set until after Seek completion)
        //4  ShellOpen     Once shell open (0=Closed, 1=Is/was Open)
        //3  IdError       (0=Okay, 1=GetID denied) (also set when Setmode.Bit4=1)
        //2  SeekError     (0=Okay, 1=Seek error)     (followed by Error Byte)
        //1  Spindle Motor (0=Motor off, or in spin-up phase, 1=Motor on)
        //0  Error         Invalid Command/parameters (followed by Error Byte)

        responseQueue.push(0); // Empty drive (?)
        //responseQueue.push(1 << 3); // GetID denied for now
        //responseQueue.push(1 << 4); // ShellOpen for now

        checkAndNotifyINT3();

    } else if (command == 0x19) { // Test
        LOG_CDROM(std::format("Command: Test"));
        uint8_t subFunction = parameterQueue.pop();

        if (subFunction == 0x20) { // Get CDROM BIOS date and version
            responseQueue.push(90); // Year
            responseQueue.push(1); // Month
            responseQueue.push(1); // Day
            responseQueue.push(1); // Version

            checkAndNotifyINT3();

        } else {
            LOG_CDROM(std::format("Subfunction 0x{:02X} not implemented", subFunction));
        }

    } else if (command == 0x1A) { // GetID
        LOG_CDROM(std::format("Command: GetID"));
        // INT3 with status first, then INT5
        responseQueue.push(0); // Status code, i.e., drive closed with no disc
        //responseQueue.push(0x11);
        //responseQueue.push(0x80);

        checkAndNotifyINT3();

        queuedResponses.push_back(&CDROM::getIDSecondResponse);

    } else {
        LOG_CDROM(std::format("Command 0x{:02X} not implemented", command));
    }
}

void CDROM::checkAndNotifyINT3() {
    LOG_CDROM(std::format("INT3 occured"));
    if ((interruptEnableRegister >> 2) & 1) {
        LOG_CDROM(std::format("Flagging and notifying about INT3"));
        interruptFlagRegister = interruptFlagRegister | 3;
        bus->interrupts.notifyAboutInterrupt(INTERRUPT_BIT_CDROM);
    } else {
        LOG_CDROM(std::format("INT3 not enabled"));
    }
}

void CDROM::checkAndNotifyINT5() {
    LOG_CDROM(std::format("INT5 occured"));
    if ((interruptEnableRegister >> 4) & 1) {
        LOG_CDROM(std::format("Flagging and notifying about INT5"));
        interruptFlagRegister = interruptFlagRegister | 5;
        bus->interrupts.notifyAboutInterrupt(INTERRUPT_BIT_CDROM);
    } else {
        LOG_CDROM(std::format("INT5 not enabled"));
    }
}

uint8_t CDROM::getStatusRegister() {
    LOG_CDROM(std::format("parameterQueue.isEmpty(): {}", parameterQueue.isEmpty()));
    LOG_CDROM(std::format("parameterQueue.isFull(): {}", parameterQueue.isFull()));
    LOG_CDROM(std::format("responseQueue.isEmpty(): {}", responseQueue.isEmpty()));
    LOG_CDROM(std::format("responseQueue.isFull(): {}", responseQueue.isFull()));
    return (0 << CDROMSTAT_BUSYSTS)
           | (0 << CDROMSTAT_DRQSTS)
           | (!responseQueue.isEmpty() << CDROMSTAT_RSLRRDY)
           | (!parameterQueue.isFull() << CDROMSTAT_PRMWRDY)
           | (parameterQueue.isEmpty() << CDROMSTAT_PRMEMPT)
           | (0 << CDROMSTAT_ADPBUSY)
           | ((statusRegister & 3) << CDROMSTAT_INDEX0);
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
        statusRegister = value & 0x3; // Only the index can be written to

    } else if (address == 0x1F801801) {
        switch (getIndex()) {
            case 0: // Command Register
                LOGV_CDROM(std::format("Write to command register: 0x{:02X}", value));
                executeCommand(value);
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
                audioVolumeRightCDOutToRightSPUInput = value;
                break;
            default:
                assert(false);
                break;
        }
    } else if (address == 0x1F801802) {
        switch (getIndex()) {
            case 0: // Parameter Queue
                LOG_CDROM(std::format("Unimplemented write to parameter queue: 0x{:02X}", value));
                // TODO Implement propertly
                parameterQueue.push(value);
                break;
            case 1: // Interrupt Enable Register
                LOG_CDROM(std::format("Unimplemented write to Interrupt Enable Register: 0x{:02X}", value));
                // TODO Implement properly
                interruptEnableRegister = value & 0x1F; // bits 5--7 are unused
                break;
            case 2: // Audio Volume for Left-CD-Out to Left-SPU-Input
                LOGV_CDROM(std::format("Write to Audio Volume for Left-CD-Out to Left-SPU-Input: 0x{:02X}", value));
                audioVolumeLeftCDOutToLeftSPUInput = value;
                break;
            case 3: // Audio Volume for Right-CD-Out to Left-SPU-Input
                LOGV_CDROM(std::format("Write to Audio Volume for Right-CD-Out to Left-SPU-Input: 0x{:02X}", value));
                audioVolumeRightCDOutToLeftSPUInput = value;
                break;
            default:
                assert(false);
                break;
        }
    } else if (address == 0x1F801803) {
        switch (getIndex()) {
            case 0: // Request Register
                LOG_CDROM(std::format("Unimplemented write to request register: 0x{:02X}", value));
                // TODO Implement
                break;
            case 1: // Interrupt Flag Register
                LOG_CDROM(std::format("Unimplemented write to Interrupt Flag Register: 0x{:02X}", value));
                // TODO Implement properly
                // Writing 1 to bits 0--4 resets them
                interruptFlagRegister = interruptFlagRegister & ~(value & 0x1F);

                // Writing 1 to bit 6 resets parameter queue
                if ((value >> 6) & 1) {
                    parameterQueue.clear();
                }

                // Clear response queue
                responseQueue.clear();

                if (!queuedResponses.empty()) {
                    QueuedResponse response = queuedResponses.front();
                    queuedResponses.pop_front();
                    (this->*response)();
                }
                break;
            case 2: // Audio Volume for Left-CD-Out to Right-SPU-Input
                LOGV_CDROM(std::format("Write to Audio Volume for Left-CD-Out to Right-SPU-Input: 0x{:02X}", value));
                audioVolumeLeftCDOutToRightSPUInput = value;
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
        //Log::loggingEnabled = true;
        value = getStatusRegister();
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
                // TODO Implement
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
                LOG_CDROM("Unimplemented read from interrupt enable register");
                // TODO implement
                break;
            case 1: // Interrupt Flag Register
            case 3: // Mirror of Interrupt Flag Register
                LOG_CDROM("Unimplemented read from interrupt flag register");
                // TODO implement
                value = interruptFlagRegister | (0xE0); // upper three bits are always 1
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

void CDROM::getIDSecondResponse() {
    LOG_CDROM(std::format("Command: GetID, 2nd response"));
    // hard-coded answer to GetID
    responseQueue.push(0x08);
    responseQueue.push(0x40);
    responseQueue.push(0x00);
    responseQueue.push(0x00);
    //responseQueue.push(0x00);
    //responseQueue.push(0x00);
    //responseQueue.push(0x00);
    //responseQueue.push(0x00);

    checkAndNotifyINT5();
}

}

