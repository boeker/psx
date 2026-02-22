#include "cdrom.h"

#include <cassert>
#include <format>

#include "exceptions/exceptions.h"
#include "util/bit.h"
#include "util/log.h"

#include "bus.h"
#include "cd.h"

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

std::string CDROM::prependState(const std::string &str) const {
    return std::format("[{:s}-{:s}] {:s}", controllerStateToString(controllerState), driveStateToString(driveState), str);
}

std::string CDROM::controllerStateToString(ControllerState controllerState) {
    switch (controllerState) {
        case IDLE:
            return "IDLE";
        case FIRST_RESPONSE:
            return "FRST";
        case SECOND_RESPONSE:
            return "SCND";
        default:
            return "INVL";
    }
}

std::string CDROM::driveStateToString(DriveState driveState) {
    switch (driveState) {
        case OPEN:
            return "OPEN";
        case NO_DISC:
            return "NDSC";
        case MOTOR_OFF:
            return "MOFF";
        case MOTOR_ON:
            return "MOON";
        case PLAYING:
            return "PLAY";
        case SEEKING:
            return "SEEK";
        case READING:
            return "READ";
        case STAY:
            return "STAY";
        default:
            return "INVL";
    }
}

uint8_t CDROM::driveStateToStatByte(DriveState driveState) {
    //7  Play          Playing CD-DA         ;\only ONE of these bits can be set
    //6  Seek          Seeking               ; at a time (ie. Read/Play won't get
    //5  Read          Reading data sectors  ;/set until after Seek completion)
    //4  ShellOpen     Once shell open (0=Closed, 1=Is/was Open)
    //3  IdError       (0=Okay, 1=GetID denied) (also set when Setmode.Bit4=1)
    //2  SeekError     (0=Okay, 1=Seek error)     (followed by Error Byte)
    //1  Spindle Motor (0=Motor off, or in spin-up phase, 1=Motor on)
    //0  Error         Invalid Command/parameters (followed by Error Byte)
    assert (driveState != INVALID);

    switch (driveState) {
        case OPEN:
            return 0x10;
        case NO_DISC:
            return 0x08; // Is that correct?
        case MOTOR_OFF:
            return 0x00;
        case MOTOR_ON:
            return 0x02;
        case PLAYING:
            return 0x82;
        case SEEKING:
            return 0x42;
        case READING:
            return 0x22;
        default:
            return 0x00;
    }
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

    firstResponse.reset();
    secondResponse.reset();

    controllerState = IDLE;
    if (cd) {
        driveState = MOTOR_ON;
    } else {
        driveState = NO_DISC;
    }
    cyclesLeft= 0;

    amm = 0;
    ass = 0;
    asect = 0;

    dataQueueBytesRemaining = 0;
}

void CDROM::setCD(std::unique_ptr<CD> cd) {
    this->cd = std::move(cd);
    driveState = MOTOR_ON;
}

void CDROM::catchUpToCPU(uint32_t cycles) {
    cyclesLeft -= std::min(cyclesLeft, cycles);
    if (cyclesLeft > 0) {
        return;
    }

    switch (controllerState) {
        case IDLE:
            break;
        case FIRST_RESPONSE:
            LOG_CDROM(prependState(std::format("========> Producing first response {:d}", secondResponse.interrupt)));
            produceResponse(firstResponse);
            break;
        case SECOND_RESPONSE:
            LOG_CDROM(prependState(std::format("========> Producing second response {:d}", secondResponse.interrupt)));
            produceResponse(secondResponse);
            break;
    }
}


void CDROM::sendCommand() {
    LOG_CDROM(prependState(std::format("Sending command 0x{:02X}", command)));
    if (Bit::getBit(requestRegister, CDROM_REQUEST_SMEN)) {
        notifyAboutINT10();
    }

    // Reset existing responses
    firstResponse.reset();
    secondResponse.reset();

    // Execute command
    (this->*commands[command])();

    scheduleFirstResponse();
}

void CDROM::scheduleFirstResponse() {
    LOG_CDROM(prependState(std::format("========> Scheduling first response {:d}", firstResponse.interrupt)));

    controllerState = FIRST_RESPONSE;
    Bit::setBit(statusRegister, CDROM_STATUS_BUSYSTS);
    cyclesLeft = firstResponse.cycles;
}

void CDROM::scheduleSecondResponse() {
    LOG_CDROM(prependState(std::format("========> Scheduling second response {:d}", secondResponse.interrupt)));

    controllerState = SECOND_RESPONSE;
    Bit::setBit(statusRegister, CDROM_STATUS_BUSYSTS);
    cyclesLeft = secondResponse.cycles;
}

void CDROM::produceResponse(const Response &response) {
    // Copy queue from response
    responseQueue = response.queue;
    Bit::clearBit(statusRegister, CDROM_STATUS_BUSYSTS);

    // Check if there is a response (commands never return INT8 or INT10)
    if (response.interrupt != 0) {
        notifyAboutINT1to7(response.interrupt);

        if (response.spam) {
            LOG_CDROM(prependState(std::format("========> Scheduling spam response {:d}", secondResponse.interrupt)));
            // Stay in current controllerState
            Bit::setBit(statusRegister, CDROM_STATUS_BUSYSTS);
            cyclesLeft = response.cycles;

        } else {
            controllerState = IDLE;
        }

        if (response.driveState != STAY) {
            LOG_CDROM(std::format("[{:s}] -> [{:s}]", driveStateToString(driveState), driveStateToString(response.driveState)));
            driveState = response.driveState;
        }
    }
}

void CDROM::notifyAboutINT1to7(uint8_t interruptNumber) {
    assert(interruptNumber >= 1 && interruptNumber <= 7);
    LOGV_CDROM(prependState(std::format("Notifying about INT{:d}", interruptNumber)));
    // Only trigger if there was no interrupt before. Is this correct?
    bool wasInterrupt = interruptFlagRegister & 0x7;
    interruptFlagRegister = interruptFlagRegister | interruptNumber;
    // Check if any of the three enable bits is enabled
    uint8_t interrupt = interruptEnableRegister & interruptFlagRegister & 0x7;
    if (!wasInterrupt && interrupt) {
        LOGV_CDROM(prependState(std::format("Issuing INT{:d}", interrupt)));
        bus->interrupts.notifyAboutInterrupt(INTERRUPT_BIT_CDROM);
    }
}

void CDROM::notifyAboutINT10() {
    LOGV_CDROM(prependState("Notifying about and issuing INT10"));
    // Only trigger if there was no interrupt before. Is this correct?
    bool wasInterrupt = Bit::getBit(interruptFlagRegister, CDROM_INTERRUPT_FLAG_CLRBFWRDY);
    Bit::setBit(interruptFlagRegister, CDROM_INTERRUPT_FLAG_CLRBFWRDY);
    if (!wasInterrupt && Bit::getBit(interruptEnableRegister, CDROM_INTERRUPT_ENABLE_BFWRDY)) {
        LOGV_CDROM(prependState("Issuing INT10"));
        bus->interrupts.notifyAboutInterrupt(INTERRUPT_BIT_CDROM);
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

    LOGT_CDROM(prependState(std::format("0x{:02X} -> @0x{:08X} with index {:d}", value, address, getIndex())));

    if (address == 0x1F801800) { // status register
        LOG_CDROM(prependState(std::format("0x{:02X} -> status register", value)));
        // Only the index can be written to
        statusRegister = (statusRegister & 0xF8) | value & 0x3;

    } else if (address == 0x1F801801) {
        switch (getIndex()) {
            case 0: // Command Register
                LOGV_CDROM(prependState(std::format("0x{:02X} -> command register", value)));
                command = value;
                sendCommand();
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
                LOGV_CDROM(std::format("0x{:02X} -> Audio Volume for Right-CD-Out to Right-SPU-Input", value));
                audioVolumeCDOutToSPUIn[3] = value;
                break;
            default:
                assert(false);
                break;
        }
    } else if (address == 0x1F801802) {
        switch (getIndex()) {
            case 0: // Parameter Queue
                LOGV_CDROM(prependState(std::format("0x{:02X} -> parameter queue", value)));
                parameterQueue.push(value);
                break;
            case 1: // Interrupt Enable Register
                LOGV_CDROM(prependState(std::format("0x{:02X} -> interrupt enable register", value)));
                interruptEnableRegister = value & 0x1F; // Bit 7 to 5 should be zero. Only use 4 to 0.
                // Should we check and issue pending interrupts here?
                break;
            case 2: // Audio Volume for Left-CD-Out to Left-SPU-Input
                LOGV_CDROM(std::format("0x{:02X} -> Audio Volume for Left-CD-Out to Left-SPU-Input", value));
                audioVolumeCDOutToSPUIn[0] = value;
                break;
            case 3: // Audio Volume for Right-CD-Out to Left-SPU-Input
                LOGV_CDROM(std::format("0x{:02X} -> Audio Volume for Right-CD-Out to Left-SPU-Input", value));
                audioVolumeCDOutToSPUIn[2] = value;
                break;
            default:
                assert(false);
                break;
        }
    } else if (address == 0x1F801803) {
        switch (getIndex()) {
            case 0: // Request Register
                LOG_CDROM(prependState(std::format("0x{:02X} -> request register", value)));
                requestRegister = value;
                if (Bit::getBit(value, CDROM_REQUEST_BFRD)) {
                    LOG_CDROM(prependState(std::format("Serving data queue", value)));
                    dataQueueBytesRemaining = cd->getSectorSize();
                } else {
                    LOG_CDROM(prependState(std::format("Resetting data queue", value)));
                    dataQueueBytesRemaining = 0;
                }
                break;
            case 1: // Interrupt Flag Register
                LOG_CDROM(prependState(std::format("0x{:02X} -> interrupt flag register", value)));
                updateInterruptFlagRegister(value);
                break;
            case 2: // Audio Volume for Left-CD-Out to Right-SPU-Input
                LOGV_CDROM(std::format("0x{:02X} -> Audio Volume for Left-CD-Out to Right-SPU-Input", value));
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
        LOGV_CDROM(prependState(std::format("status register -> 0x{:02X}", value)));

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

                LOGV_CDROM(prependState(std::format("response queue -> 0x{:02X}", value)));
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
                if (dataQueueBytesRemaining > 0) {
                    value = cd->readByte();
                } else {
                    LOG_CDROM("Read from empty data queue");
                }
                --dataQueueBytesRemaining;
                LOGT_CDROM(std::format("data queue -> 0x{:02X}", value));
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
                LOGV_CDROM(prependState(std::format("interrupt enable register -> 0x{:02X}", value)));
                break;
            case 1: // Interrupt Flag Register
            case 3: // Mirror of Interrupt Flag Register
                value = interruptFlagRegister | 0xE0; // Bits 7 to 5 are always 1
                LOGV_CDROM(prependState(std::format("interrupt flag register -> 0x{:02X}", value)));
                break;
            default:
                assert(false);
                break;
        }

    } else {
        LOG_CDROM(std::format("Unimplemented read @0x{:08X} with index {:d}", address, getIndex()));
    }

    LOGT_CDROM(prependState(std::format("@0x{:08X} with index {:d} -> 0x{:02X}", address, getIndex(), value)));

    return value;
}

uint8_t CDROM::getIndex() const {
    return statusRegister & 0x3;
}

void CDROM::updateStatusRegister() {
    Bit::clearBit(statusRegister, CDROM_STATUS_BUSYSTS);
    Bit::setBit(statusRegister, CDROM_STATUS_DRQSTS, dataQueueBytesRemaining > 0);
    Bit::setBit(statusRegister, CDROM_STATUS_RSLRRDY, !responseQueue.isEmpty());
    Bit::setBit(statusRegister, CDROM_STATUS_PRMWRDY, !parameterQueue.isFull());
    Bit::setBit(statusRegister, CDROM_STATUS_PRMEMPT, parameterQueue.isEmpty());
    Bit::setBit(statusRegister, CDROM_STATUS_ADPBUSY, 0); // TODO XA-ADPCM
}

void CDROM::updateInterruptFlagRegister(uint8_t value) {
    //LOG_CDROM(prependState("Updating flag register"));
    // 7 - CHPRST: Unknown

    // 6 - CLRPRM: Reset Parameter Queue
    if (Bit::getBit(value, CDROM_INTERRUPT_FLAG_CLRPRM)) {
        LOG_CDROM(prependState(std::format("Resetting parameter queue")));
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
    //LOG_CDROM(std::format("Was interrupt active before: {:s}, is interrupt active now: {:s}", wasInterrupt, isInterrupt));

    // Acknowledge empties response queue, sends pending command (if there is one)
    // But what counts as an "acknowledge"?
    // Would clearing a single bit of, say, INT3 suffice?
    // And what about INT10 and INT8?
    // Let's assume that we only consider INT1...7 and that it has to be cleared completely
    if (wasInterrupt && !isInterrupt) {
        LOG_CDROM(prependState("ACK"));
        // Clear response queue
        responseQueue.clear();

        // Check if there is a second response waiting
        if (secondResponse.interrupt != 0) {
            scheduleSecondResponse();
        }
    }
}

const CDROM::Command CDROM::commands[] = {
    // 0x00
    &CDROM::Unknown,
    &CDROM::Getstat, // 0x01
    &CDROM::Setloc, // 0x02
    &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::ReadN,
    &CDROM::Unknown,
    &CDROM::Stop, // 0x08
    &CDROM::Pause, // 0x09
    &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Setmode, //0x0E
    &CDROM::Unknown,
    // 0x10
    &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown,
    &CDROM::SeekL, // 0x15
    &CDROM::Unknown, &CDROM::Unknown,
    &CDROM::Unknown,
    &CDROM::Test, // 0x19
    &CDROM::GetID, // 0x1A
    &CDROM::Unknown,
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
    LOG_CDROM(prependState(std::format("========> Getstat() <========")));
    //7  Play          Playing CD-DA         ;\only ONE of these bits can be set
    //6  Seek          Seeking               ; at a time (ie. Read/Play won't get
    //5  Read          Reading data sectors  ;/set until after Seek completion)
    //4  ShellOpen     Once shell open (0=Closed, 1=Is/was Open)
    //3  IdError       (0=Okay, 1=GetID denied) (also set when Setmode.Bit4=1)
    //2  SeekError     (0=Okay, 1=Seek error)     (followed by Error Byte)
    //1  Spindle Motor (0=Motor off, or in spin-up phase, 1=Motor on)
    //0  Error         Invalid Command/parameters (followed by Error Byte)

    if (driveState == NO_DISC) {
        firstResponse.interrupt = 3;
        firstResponse.queue.push(0x08); // Empty drive (?)
    } else {
        firstResponse.interrupt = 3;
        firstResponse.queue.push(0x02);
    }
}

void CDROM::Setloc() {
    amm = parameterQueue.pop();
    ass = parameterQueue.pop();
    asect = parameterQueue.pop();

    LOG_CDROM(prependState(std::format("========> Setloc(0x{:02}, 0x{:02}, 0x{:02}) <========", amm, ass, asect)));

    firstResponse.interrupt = 3;
    firstResponse.queue.push(0x02);
}

void CDROM::ReadN() {
    LOG_CDROM(prependState(std::format("========> ReadN() <========")));

    firstResponse.interrupt = 3;
    firstResponse.setAndPush(READING);

    secondResponse.interrupt = 1;
    secondResponse.setAndPush(MOTOR_ON);
    secondResponse.cycles = 0x36CD2;
}

void CDROM::Stop() {
    LOG_CDROM(prependState(std::format("========> Stop() <========")));

    firstResponse.interrupt = 3;
    firstResponse.setAndPush(driveState); // Current state

    secondResponse.interrupt = 2;
    secondResponse.setAndPush(MOTOR_OFF);
}

void CDROM::Pause() {
    LOG_CDROM(prependState(std::format("========> Pause() <========")));

    firstResponse.interrupt = 3;
    firstResponse.setAndPush(driveState); // Old state

    secondResponse.interrupt = 2;
    secondResponse.setAndPush(MOTOR_ON); // Not reading anymore
}

void CDROM::Setmode() {
    uint8_t mode = parameterQueue.pop();
    LOG_CDROM(prependState(std::format("========> Setmode(0x{:02X}) <========", mode)));
    // TODO Implement properly

    firstResponse.interrupt = 3;
    firstResponse.setAndPush(driveState);
}

void CDROM::SeekL() {
    LOG_CDROM(prependState(std::format("========> SeekL() <========")));

    cd->seekTo(amm, ass, asect);

    firstResponse.interrupt = 3;
    firstResponse.setAndPush(SEEKING);

    secondResponse.interrupt = 2;
    secondResponse.setAndPush(MOTOR_ON);
}

void CDROM::Test() {
    LOG_CDROM(prependState(std::format("========> Test() <========")));
    function = parameterQueue.pop();

    // Execute sub-function
    (this->*subFunctions[function])();
}

void CDROM::GetID() {
    LOG_CDROM(prependState(std::format("========> GetID() <========")));
    // INT3 with status first, then INT5

    if (driveState == NO_DISC) {
        firstResponse.interrupt = 3;
        firstResponse.queue.push(0x08); // Stat again, i.e., drive closed with no disc

        secondResponse.interrupt = 5;
        secondResponse.queue.push(0x08);
        secondResponse.queue.push(0x40);
        secondResponse.queue.push(0x00);
        secondResponse.queue.push(0x00);
        secondResponse.queue.push(0x00);
        secondResponse.queue.push(0x00);
        secondResponse.queue.push(0x00);
        secondResponse.queue.push(0x00);
    } else {
        // Licensed Mode 2
        firstResponse.interrupt = 3;
        firstResponse.queue.push(0x00); // Stat again, i.e., Motor off

        secondResponse.interrupt = 2;
        secondResponse.queue.push(0x02); // stat
        secondResponse.queue.push(0x00); // flags
        secondResponse.queue.push(0x20); // type
        secondResponse.queue.push(0x00); // atip
        secondResponse.queue.push(0x53); // S
        secondResponse.queue.push(0x43); // C
        secondResponse.queue.push(0x45); // E
        secondResponse.queue.push(0x41); // A
    }
}

void CDROM::UnknownSF() {
    throw exceptions::UnknownCDROMFunctionError(std::format("Unknown function 0x{:02X}", function));
}

void CDROM::Function0x20() {
    LOG_CDROM(prependState(std::format("========> Function: 0x20 <========")));

    // hard-coded answer
    firstResponse.interrupt = 3;
    firstResponse.queue.push(0x95); // Year
    firstResponse.queue.push(0x05); // Month
    firstResponse.queue.push(0x16); // Day
    firstResponse.queue.push(0xC1); // Version
}

}

