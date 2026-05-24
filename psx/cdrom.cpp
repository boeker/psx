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
    return std::format("[{:s}-{:s}] {:s}", controllerStateToString(controllerState), driveStateToString(drive_state), str);
}

std::string CDROM::controllerStateToString(ControllerState controllerState) {
    switch (controllerState) {
        case IDLE:
            return "IDLE";
        case BUSY:
            return "BUSY";
        default:
            return "INVL";
    }
}

std::string CDROM::driveStateToString(DriveState driveState) {
    switch (driveState) {
        case OPEN:
            return "OPEN";
        case MOTOR_OFF:
            return "MOFF";
        case MOTOR_ON:
            return "SPIN";
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
    switch (driveState) {
        case OPEN:
            return 0x11;
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

    current_sector_buffer = new uint8_t[SECTOR_SIZE];
    next_sector_buffer = new uint8_t[SECTOR_SIZE];

    reset();
    if (cd) {
        cd->reset();
    }
}

CDROM::~CDROM() {
    delete[] current_sector_buffer;
    delete[] next_sector_buffer;
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

    pending = false;
    command = 0;
    function = 0;
    parameterQueue.clear();
    parameter_queue.clear();

    firstResponse.reset();
    secondResponse.reset();

    controller_state = IDLE;
    drive_state = MOTOR_OFF;
    cycles_left= 0;

    next_sector_buffer_ready = false;

    amm = 0;
    ass = 0;
    asect = 0;

    mode = 0;
    sector_offset = 0;
    sector_end = 0;
}

void CDROM::setCD(std::unique_ptr<CD> cd) {
    this->cd = std::move(cd);
    drive_state = MOTOR_ON;
}

CD& CDROM::getCD() {
    return *cd;
}

void CDROM::catchUpToCPU(uint32_t cycles) {
    cyclesLeft -= std::min(cyclesLeft, cycles);
    if (cyclesLeft > 0) {
        return;
    }

    switch (controller_state) {
        case IDLE:
            break;
        case BUSY:
            break;
        case FIRST_RESPONSE:
            LOG_CDROM(prependState(std::format("========> Producing first response {:d}", firstResponse.interrupt)));
            deliverResponse(firstResponse);
            break;
        case SECOND_RESPONSE:
            LOG_CDROM(prependState(std::format("========> Producing second response {:d}", secondResponse.interrupt)));
            deliverResponse(secondResponse);
            break;
    }
}


void CDROM::sendCommand() {
    if (controller_state == IDLE || (controller_state == SECOND_RESPONSE && secondResponse.delivered)) {
        pending = false;
        LOG_CDROM(prependState(std::format("Sending command 0x{:02X}", command)));
        if (Bit::getBit(requestRegister, CDROM_REQUEST_SMEN)) {
            notifyAboutINT10();
        }

        // Reset existing responses
        firstResponse.reset();
        secondResponse.reset();

        // Actually send command / schedule response
        LOG_CDROM(prependState(std::format("========> Scheduling Response: 0x{:02X}() <========", command)));
        scheduled_responses.emplace_back(commands[command]);

        controller_state = BUSY;
        Bit::setBit(statusRegister, CDROM_STATUS_BUSYSTS);
        // TODO Set cycles_left to that of response

        //scheduleFirstResponse();
    }
}

void CDROM::scheduleFirstResponse() {
    LOG_CDROM(prependState(std::format("========> Scheduling first response {:d}", firstResponse.interrupt)));

    controller_state = FIRST_RESPONSE;
    Bit::setBit(statusRegister, CDROM_STATUS_BUSYSTS);
    cyclesLeft = firstResponse.cycles;
}

void CDROM::scheduleSecondResponse() {
    LOG_CDROM(prependState(std::format("========> Scheduling second response {:d}", secondResponse.interrupt)));

    controller_state = SECOND_RESPONSE;
    Bit::setBit(statusRegister, CDROM_STATUS_BUSYSTS);
    cyclesLeft = secondResponse.cycles;
}

void CDROM::deliver_response(ScheduledResponse &response) {
    // Save old drive state for logging purposes
    DriveState old_state = drive_state;

    // Execute response function
    uint8_t interrupt = (this->*response.function)();

    Bit::clearBit(statusRegister, CDROM_STATUS_BUSYSTS);

    if (interrupt == 0) {
        LOG_CDROM(std::format("Warning: Response function returned 0"));
        return;
    }

    notifyAboutINT1to7(interrupt);

    // Executing the response function might have scheduled a new response
    if (!scheduled_responses.empty()) {
        controller_state = BUSY;
        Bit::setBit(statusRegister, CDROM_STATUS_BUSYSTS);
        // TODO set cycles_left
    } else {
        controller_state = IDLE;
    }

    if (drive_state != old_state) {
        LOG_CDROM(std::format("[{:s}] -> [{:s}]", driveStateToString(old_state), driveStateToString(drive_state)));
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
        LOGT_CDROM(prependState(std::format("0x{:02X} -> status register", value)));
        // Only the index can be written to
        statusRegister = (statusRegister & 0xF8) | value & 0x3;

    } else if (address == 0x1F801801) {
        switch (getIndex()) {
            case 0: // Command Register
                LOGV_CDROM(prependState(std::format("@0x{:08X}: 0x{:02X} -> command register", bus->cpu.instructionPC, value)));
                command = value;
                pending = true;
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
                parameter_queue.push(value);
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
                    if (cd) {
                        if (next_sector_buffer_ready) {
                            next_sector_buffer_ready = false;
                            std::swap(current_sector_buffer, next_sector_buffer);
                            bool large_sector_size = mode & (1U << CDROM_MODE_SECTOR_SIZE);
                            sector_offset = large_sector_size ? CD_MODE2_SYNC_BYTES : CD_MODE2_DATA_OFFSET;
                            sector_end = large_sector_size ? CD::SECTOR_SIZE : (CD::SECTOR_SIZE - 0x118);
                        } else {
                            LOG_CDROM(prependState(std::format("Warning: No ready sector was read, cannot serve")));
                        }
                    }
                } else {
                    LOG_CDROM(prependState(std::format("Resetting data queue", value)));
                    sector_offset = 0;
                    sector_end = 0;
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

                LOGV_CDROM(prependState(std::format("@0x{:08X}: response queue -> 0x{:02X}", bus->cpu.instructionPC, value)));
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
                if (has_data()) {
                    value = read_byte();
                } else {
                    LOG_CDROM("Read from empty data queue");
                }
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
                LOGT_CDROM(prependState(std::format("interrupt enable register -> 0x{:02X}", value)));
                break;
            case 1: // Interrupt Flag Register
            case 3: // Mirror of Interrupt Flag Register
                value = interruptFlagRegister | 0xE0; // Bits 7 to 5 are always 1
                LOGT_CDROM(prependState(std::format("interrupt flag register -> 0x{:02X}", value)));
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
    Bit::setBit(statusRegister, CDROM_STATUS_DRQSTS, has_data());
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
        parameter_queue.clear();
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
    bool wasInterrupt = interruptFlagRegister & 0x7;
    interruptFlagRegister = interruptFlagRegister & ~(value & 0x7);
    bool isInterrupt = interruptFlagRegister & 0x7;
    //LOG_CDROM(std::format("Was interrupt active before: {:s}, is interrupt active now: {:s}", wasInterrupt, isInterrupt));

    // Acknowledge empties response queue, sends pending command (if there is one)
    // But what counts as an "acknowledge"?
    // Would clearing a single bit of, say, INT3 suffice?
    // And what about INT10 and INT8?
    // Let's assume that we only consider INT1...7 and that it has to be cleared completely
    if (wasInterrupt && !isInterrupt) {
        LOG_CDROM(prependState(std::format("Acknowledged interrupt: pending = {:s}", pending)));
        // Clear response queue
        // TODO Do we have to clear this or is the user responsible for this?
        //responseQueue.clear();

        if (pending && (secondResponse.interrupt == 0 || secondResponse.delivered)) {
            sendCommand();
        } else {
            // Check if there is a second response waiting
            if (secondResponse.interrupt != 0 && !secondResponse.delivered) {
                scheduleSecondResponse();
            }
        }
    }
}

bool CDROM::has_data() {
    return !sector_buffer.empty() && sector_offset < sector_end;
}

uint8_t CDROM::read_byte() {
    return sector_buffer[sector_offset++];
}

uint32_t CDROM::read_word() {
    uint32_t value = *(reinterpret_cast<const uint32_t*>(&sector_buffer[sector_offset]));
    sector_offset += 4;
    return value;
}

const CDROM::Command CDROM::commands[] = {
    // 0x00
    &CDROM::unknown,
    &CDROM::get_stat, // 0x01
    &CDROM::set_loc, // 0x02
    &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown,
    &CDROM::read_n,
    &CDROM::unknown,
    &CDROM::stop, // 0x08
    &CDROM::pause, // 0x09
    &CDROM::init, // 0x0A
    &CDROM::unknown,
    &CDROM::demute, // 0x0C
    &CDROM::unknown,
    &CDROM::set_mode, //0x0E
    &CDROM::unknown,
    // 0x10
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::get_tn, // 0x13
    &CDROM::get_td, // 0x14
    &CDROM::seek_l, // 0x15
    &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown,
    &CDROM::test, // 0x19
    &CDROM::get_id, // 0x1A
    &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown,
    &CDROM::read_toc,
    &CDROM::unknown,
    // 0x20
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    // 0x30
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    // 0x40
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    // 0x50
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    // 0x60
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    // 0x70
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    // 0x80
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    // 0x90
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    // 0xA0
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    // 0xB0
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    // 0xC0
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    // 0xD0
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    // 0xE0
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    // 0xF0
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown,
    &CDROM::unknown, &CDROM::unknown, &CDROM::unknown, &CDROM::unknown
};

const CDROM::ResponseFunction CDROM::sub_functions[] = {
    // 0x00
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    // 0x10
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    // 0x20
    &CDROM::function_0x20,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    // 0x30
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    // 0x40
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    // 0x50
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    // 0x60
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    // 0x70
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    // 0x80
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    // 0x90
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    // 0xA0
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    // 0xB0
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    // 0xC0
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    // 0xD0
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    // 0xE0
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    // 0xF0
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf,
    &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf, &CDROM::unknown_sf
};

void CDROM::push_drive_state_to_response_queue() {
    response_queue.push(driveStateToStatByte(drive_state));
}

uint8_t CDROM::no_disc_response() {
    response_queue.push(0x01);
    response_queue.push(0x80);
    return 5;
}

void CDROM::unknown() {
    throw exceptions::UnknownCDROMCommandError(std::format("Unknown command 0x{:02X}", command));
}

uint8_t CDROM::get_stat() {
    LOG_CDROM(prependState(std::format("========> Getstat() <========")));
    //7  Play          Playing CD-DA         ;\only ONE of these bits can be set
    //6  Seek          Seeking               ; at a time (ie. Read/Play won't get
    //5  Read          Reading data sectors  ;/set until after Seek completion)
    //4  ShellOpen     Once shell open (0=Closed, 1=Is/was Open)
    //3  IdError       (0=Okay, 1=GetID denied) (also set when Setmode.Bit4=1)
    //2  SeekError     (0=Okay, 1=Seek error)     (followed by Error Byte)
    //1  Spindle Motor (0=Motor off, or in spin-up phase, 1=Motor on)
    //0  Error         Invalid Command/parameters (followed by Error Byte)

    if (drive_state == OPEN) {
        response_queue.push(0x11);
        response_queue.push(0x80);
        return 5;
    }

    if (!cd) {
        response_queue.push(0x00);
        return 3;
    }

    if (drive_state == MOTOR_OFF) {
        response_queue.push(0x00);
        return 3;
    }

    response_queue.push(0x02);
    return 3;
}

uint8_t CDROM::set_loc() {
    amm = parameter_queue.pop();
    ass = parameter_queue.pop();
    asect = parameter_queue.pop();

    LOG_CDROM(prependState(std::format("========> Setloc(0x{:02X}, 0x{:02X}, 0x{:02X}) <========", amm, ass, asect)));

    if (!cd) {
        return no_disc_response();
    }

    response_queue.push(0x02);
    return 3;
}

uint8_t CDROM::read_n() {
    LOG_CDROM(prependState(std::format("========> ReadN(): Initial Response <========")));

    if (!cd) {
        return no_disc_response();
    }

    // Write response
    drive_state = READING;
    push_drive_state_to_response_queue();

    // Schedule second response
    scheduled_responses.emplace_back(&CDROM::read_n_second, 0x36CD2);

    return 3;
}

uint8_t CDROM::read_n_second() {
    LOG_CDROM(prependState(std::format("========> ReadN(): Second Response <========")));

    // Read sector from CD
    if (next_sector_buffer_ready) {
        LOG_CDROM(prependState(std::format("Warning: About to overwrite unserved sector!")));
    }
    cd->read_sector_and_advance(next_sector_buffer);
    next_sector_buffer_ready = true;

    // Update drive state
    drive_state = READING;
    push_drive_state_to_response_queue();

    // Schedule reading of next sector (since we are automatically reading that)
    // Software has to be fast enough to keep up!
    // That is, the next_sector_buffer has to be read or we will overwrite it.
    scheduled_responses.emplace_back(&CDROM::read_n_second, 0x36CD2);

    return 1;
}

uint8_t CDROM::stop() {
    LOG_CDROM(prependState(std::format("========> Stop(): Initial Response <========")));

    if (!cd) {
        return no_disc_response();
    }

    // Schedule second response
    scheduled_responses.emplace_back(&CDROM::stop_second);

    push_drive_state_to_response_queue(); // Respond with current state
    return 3;
}

uint8_t CDROM::stop_second() {
    LOG_CDROM(prependState(std::format("========> Stop(): Second Response <========")));

    drive_state = MOTOR_OFF;
    push_drive_state_to_response_queue();
    return 2;
}

uint8_t CDROM::pause() {
    LOG_CDROM(prependState(std::format("========> Pause(): Initial Response <========")));

    if (!cd) {
        return no_disc_response();
    }

    // Schedule second response
    scheduled_responses.emplace_back(&CDROM::pause_second);

    push_drive_state_to_response_queue(); // Respond with current state
    return 3;
}

uint8_t CDROM::pause_second() {
    LOG_CDROM(prependState(std::format("========> Pause(): Second Response <========")));

    drive_state = MOTOR_ON;
    push_drive_state_to_response_queue();
    return 2;
}

uint8_t CDROM::init() {
    LOG_CDROM(prependState(std::format("========> Init(): Initial Response <========")));

    // TODO set mode to 0x20
    scheduled_responses.emplace_back(&CDROM::init_second);

    push_drive_state_to_response_queue(); // Current/old state
    return 3;
}

uint8_t CDROM::init_second() {
    LOG_CDROM(prependState(std::format("========> Init(): Second Response <========")));

    drive_state = MOTOR_ON;
    push_drive_state_to_response_queue();
    return 2;
}

uint8_t CDROM::demute() {
    LOG_CDROM(prependState(std::format("========> Demute() <========")));

    // TODO Do something?

    push_drive_state_to_response_queue();
    return 3;
}

uint8_t CDROM::setmode() {
    mode = parameterQueue.pop();
    LOG_CDROM(prependState(std::format("========> Setmode(0x{:02X}) <========", mode)));

    // TODO Handle all bits

    push_drive_state_to_response_queue();
    return 3;
}

uint8_t CDROM::get_tn() {
    LOG_CDROM(prependState(std::format("========> GetTN() <========")));

    if (!cd) {
        return no_disc_response();
    }

    push_drive_state_to_response_queue();
    response_queue.push(0x01); // first TODO do not use hardcoded value
    response_queue.push(0x01); // last TODO do not use hardcoded value
    return 3;
}

uint8_t CDROM::get_td() {
    uint8_t track = parameterQueue.pop();
    LOG_CDROM(prependState(std::format("========> GetTD(0x{:02X}) <========", track)));

    if (!cd) {
        return no_disc_response();
    }

    push_drive_state_to_response_queue();
    response_queue.push(0x00); // first TODO do not use hardcoded value
    response_queue.push(0x00); // last TODO do not use hardcoded value
    return 3;
}

uint8_t CDROM::seek_l() {
    LOG_CDROM(prependState(std::format("========> SeekL(): Initial Response <========")));

    if (!cd) {
        firstResponse.setNoDisc();
        return;
    }

    scheduled_responses.emplace_back(&CDROM::seek_l_second);

    drive_state = SEEKING;
    push_drive_state_to_response_queue();
    return 3;
}

uint8_t CDROM::seek_l_second() {
    LOG_CDROM(prependState(std::format("========> SeekL(): Second Response <========")));

    cd->seek_to_bcd(amm, ass, asect);

    drive_state = MOTOR_ON;
    push_drive_state_to_response_queue();
    return 2;
}

uint8_t CDROM::test() {
    LOG_CDROM(prependState(std::format("========> Test() <========")));
    function = parameterQueue.pop();

    // Execute sub-function
    return (this->*subFunctions[function])();
}

uint8_t CDROM::get_id() {
    LOG_CDROM(prependState(std::format("========> GetID(): Initial Response <========")));

    if (drive_state == OPEN) {
        response_queue.push(0x11);
        response_queue.push(0x80);
        return 5;
    }

    if (!cd) {
        scheduled_responses.emplace_back(&CDROM::get_id_second_motor_off);
        response_queue.push(0x00);
        return 3;
    }

    if (drive_state == MOTOR_OFF) {
        scheduled_responses.emplace_back(&CDROM::get_id_second_motor_off);
        response_queue.push(0x00);
        return 3;

    }

    // Licensed Mode 2
    scheduled_responses.emplace_back(&CDROM::get_id_second_mode_2);
    response_queue.push(0x02);
    return 3;
}

uint8_t CDROM::get_id_second_motor_off() {
    LOG_CDROM(prependState(std::format("========> GetID(): Second Response (Motor Off) <========")));

    response_queue.push(0x08); // Also for no disc?
    response_queue.push(0x40);
    response_queue.push(0x00);
    response_queue.push(0x00);
    response_queue.push(0x00);
    response_queue.push(0x00);
    response_queue.push(0x00);
    response_queue.push(0x00);
    return 5;
}

uint8_t CDROM::get_id_second_mode_2() {
    LOG_CDROM(prependState(std::format("========> GetID(): Second Response (Licensed Disc, Mode 2) <========")));

    response_queue.push(0x02); // stat
    response_queue.push(0x00); // flags
    response_queue.push(0x20); // type
    response_queue.push(0x00); // atip
    response_queue.push(0x53); // S
    response_queue.push(0x43); // C
    response_queue.push(0x45); // E
    response_queue.push(0x41); // A
    return 2;
}

uint8_t CDROM::read_toc() {
    LOG_CDROM(prependState(std::format("========> ReadTOC(): Initial Response <========")));
    // INT3 with status first, then INT2 with status

    scheduled_responses.emplace_back(&CDROM::read_toc_second, 0x1F78A40);
    push_drive_state_to_response_queue();
    return 3;
}

uint8_t CDROM::read_toc_second() {
    LOG_CDROM(prependState(std::format("========> ReadTOC(): Second Response <========")));

    push_drive_state_to_response_queue();
    return 2;
}

void CDROM::unknown_sf() {
    throw exceptions::UnknownCDROMFunctionError(std::format("Unknown function 0x{:02X}", function));
}

void CDROM::function_0x20() {
    LOG_CDROM(prependState(std::format("========> Function: 0x20 <========")));

    // hard-coded answer
    response_queue.push(0x99); // Year
    response_queue.push(0x02); // Month
    response_queue.push(0x01); // Day
    response_queue.push(0xC3); // Version
    return 3;
}

}

