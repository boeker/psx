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
    index = 0;
    interruptEnableRegister = 0;
    interruptFlagRegister = 0;

    parameterQueue.clear();
    responseQueue.clear();
    queuedInterrupt = 0;
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

        responseQueue.push(1 << 3); // GetID denied for now
        //responseQueue.push(1 << 4); // ShellOpen for now

        // INT3
        if ((interruptEnableRegister >> 2) & 1) {
            interruptFlagRegister = interruptFlagRegister | 3;
            bus->interrupts.notifyAboutInterrupt(INTERRUPT_BIT_CDROM);
        }

    } else if (command == 0x19) { // Test
        LOG_CDROM(std::format("Command: Test"));
        uint8_t subFunction = parameterQueue.pop();

        if (subFunction == 0x20) { // Get CDROM BIOS date and version
            responseQueue.push(90);
            responseQueue.push(1);
            responseQueue.push(1);
            responseQueue.push(1);

            // INT3
            if ((interruptEnableRegister >> 2) & 1) {
                interruptFlagRegister = interruptFlagRegister | 3;
                bus->interrupts.notifyAboutInterrupt(INTERRUPT_BIT_CDROM);
            }

        } else {
            LOG_CDROM(std::format("Subfunction 0x{:02X} not implemented", subFunction));
        }

    } else if (command == 0x1A) { // GetID
            // INT3 with status first, then INT5
            responseQueue.push(0);
            //responseQueue.push(0x11);
            //responseQueue.push(0x80);

            // INT3
            if ((interruptEnableRegister >> 2) & 1) {
                interruptFlagRegister = interruptFlagRegister | 3;
                bus->interrupts.notifyAboutInterrupt(INTERRUPT_BIT_CDROM);
            }
            //if ((interruptEnableRegister >> 4) & 1) {
            //    interruptFlagRegister = interruptFlagRegister | 5;
            //    bus->interrupts.notifyAboutInterrupt(INTERRUPT_BIT_CDROM);
            //}

            queuedInterrupt =  5;

    } else {
        LOG_CDROM(std::format("Command 0x{:02X} not implemented", command));
    }
}

uint8_t CDROM::getStatusRegister() {
    return (0 << CDROMSTAT_BUSYSTS)
           | (0 << CDROMSTAT_DRQSTS)
           | (!responseQueue.isEmpty() << CDROMSTAT_RSLRRDY)
           | (!parameterQueue.isFull() << CDROMSTAT_PRMWRDY)
           | (parameterQueue.isEmpty() << CDROMSTAT_PRMEMPT)
           | (0 << CDROMSTAT_ADPBUSY)
           | ((index & 3) << CDROMSTAT_INDEX0);
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

    if (address == 0x1F801800) { // status register
        LOG_CDROM(std::format("Write to status register: 0x{:0{}X}", value, 2*sizeof(value)));
        index = value & 3; // only the index can be written to

    } else if (address == 0x1F801801) {
        if (index == 0) { // command register
            LOG_CDROM(std::format("Write to command register: 0x{:0{}X}", value, 2*sizeof(value)));
            executeCommand(value);

        } else {
            LOG_CDROM(std::format("Unimplemented write 0x{:0{}X} -> @0x{:08X}, index {:d}", value, 2*sizeof(value), address, index));
        }
    } else if (address == 0x1F801802) {
        if (index == 0) { // parameter queue
            LOG_CDROM(std::format("Write to parameter queue: 0x{:0{}X}", value, 2*sizeof(value)));
            parameterQueue.push(value);

        } else if (index == 1) { // interrupt enable register
            LOG_CDROM(std::format("Write to interrupt enable register: 0x{:0{}X}", value, 2*sizeof(value)));
            interruptEnableRegister = value & 0x1F; // bits 5--7 are unused

        } else {
            LOG_CDROM(std::format("Unimplemented write 0x{:0{}X} -> @0x{:08X}, index {:d}", value, 2*sizeof(value), address, index));
        }

    } else if (address == 0x1F801803) {
        if (index == 1) { // interrupt flag register
            LOG_CDROM(std::format("Write to interrupt flag register: 0x{:0{}X}", value, 2*sizeof(value)));
            // Writing 1 to bits 0--4 resets them
            interruptFlagRegister = interruptFlagRegister & ~(value & 0x1F);

            // Writing 1 to bit 6 resets parameter queue
            if ((value >> 6) & 1) {
                parameterQueue.clear();
            }

            // Clear response queue
            responseQueue.clear();

            if (queuedInterrupt != 0) {
                // INT5 for now

                LOG_CDROM(std::format("Writing 2nd response"));
                // hard-coded answer to GetID
                responseQueue.push(0x08);
                responseQueue.push(0x40);
                responseQueue.push(0x00);
                responseQueue.push(0x00);
                responseQueue.push(0x00);
                responseQueue.push(0x00);
                responseQueue.push(0x00);
                responseQueue.push(0x00);

                if ((interruptEnableRegister >> 4) & 1) {
                    interruptFlagRegister = interruptFlagRegister | 5;
                    bus->interrupts.notifyAboutInterrupt(INTERRUPT_BIT_CDROM);
                }

                queuedInterrupt = 0;
            }

        } else {
            LOG_CDROM(std::format("Unimplemented write 0x{:0{}X} -> @0x{:08X}, index {:d}", value, 2*sizeof(value), address, index));
        }

    } else {
        LOG_CDROM(std::format("Unimplemented write 0x{:0{}X} -> @0x{:08X}, index {:d}", value, 2*sizeof(value), address, index));
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

        LOG_CDROM(std::format("Read from status register: 0x{:0{}X}", value, 2*sizeof(value)));

    } else if (address == 0x1F801801) {
        if (index == 1) { // response
            if (responseQueue.isEmpty()) {
                LOG_CDROM(std::format("Response queue is empty!"));
                value = 0;

            } else {
                value = responseQueue.pop();
            }

            LOG_CDROM(std::format("Reading response: 0x{:02X}", value));

        }  else {
            LOG_CDROM(std::format("Unimplemented read @0x{:08X}, index {:d}", address, index));
        }

    } else if (address == 0x1F801803) {
        if (index == 1) { // interrupt flag register
            return interruptFlagRegister | (0xE0); // upper three bits are always 1

        }  else {
            LOG_CDROM(std::format("Unimplemented read @0x{:08X}, index {:d}", address, index));
        }

    } else {
        LOG_CDROM(std::format("Unimplemented read @0x{:08X}, index {:d}", address, index));
    }

    return value;
}

}

