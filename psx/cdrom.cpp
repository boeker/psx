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

std::string CDROM::get_state_prefix() const {
    return std::format("[{:s}]", driveStateToString(drive_state));
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

    reset();
    if (cd) {
        cd->reset();
    }
}

CDROM::~CDROM() {
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
    pending_command = false;
    function = 0;
    parameter_queue.clear();

    drive_state = MOTOR_OFF;

    waiting_for_acknowledge = false;
    scheduled_responses.clear();
    cycles_left= 0;
    response_queue.clear();

    current_sector_buffer.reset(nullptr);
    read_sector_buffers.clear();
    unused_sector_buffers.clear();

    // Let us start with three sector buffers
    unused_sector_buffers.emplace_back(std::make_unique<uint8_t[]>(CD::SECTOR_SIZE));
    unused_sector_buffers.emplace_back(std::make_unique<uint8_t[]>(CD::SECTOR_SIZE));
    unused_sector_buffers.emplace_back(std::make_unique<uint8_t[]>(CD::SECTOR_SIZE));

    last_sector_header.reset();

    amm = 0;
    ass = 0;
    asect = 0;
    set_loc_unprocessed = false;

    filter_file = 0;
    filter_channel = 0;

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
    cycles_left -= std::min(cycles_left, cycles);
    if (cycles_left > 0) {
        return;
    }

    if (!scheduled_responses.empty() && !waiting_for_acknowledge) {
        LOGL_DSC(LogLevel::TRACE, "{:s} ========> Delivering Response", get_state_prefix());
        ScheduledResponse response = scheduled_responses.front();
        scheduled_responses.pop_front();
        deliver_response(response);
    }
}

void CDROM::deliver_response(ScheduledResponse &response) {
    // Save old drive state for logging purposes
    DriveState old_state = drive_state;

    // Execute response function
    uint8_t interrupt = (this->*response.function)();
    if (interrupt != 0) { // 0 is used to communicate that no interrupt should be issued
        waiting_for_acknowledge = true;
        notifyAboutINT1to7(interrupt);
    }

    // Executing the response function might have scheduled a new response
    // Or there might be some responses left
    if (!scheduled_responses.empty()) {
        cycles_left = scheduled_responses.begin()->cycles;
    }

    if (drive_state != old_state) {
        LOGL_DSC(LogLevel::DEBUG, "[{:s}] -> [{:s}]", driveStateToString(old_state), driveStateToString(drive_state));
    }
}


void CDROM::send_command() {
    if (pending_command && (scheduled_responses.empty() || drive_state == READING || drive_state == PLAYING)) { // Let Pause() get through
        pending_command = false;

        LOGL_DSC(LogLevel::DEBUG, "{:s} Sending command 0x{:02X} to controller", get_state_prefix(), command);
        if (Bit::getBit(requestRegister, CDROM_REQUEST_SMEN)) {
            notifyAboutINT10();
        }

        // Execute command
        (this->*commands[command])();

        // Every command has to schedule a response
        assert(!scheduled_responses.empty());
        cycles_left = scheduled_responses.front().cycles;
    }
}

void CDROM::notifyAboutINT1to7(uint8_t interruptNumber) {
    assert(interruptNumber >= 1 && interruptNumber <= 7);
    LOGL_DSC(LogLevel::DEBUG, "{:s} Notifying about INT{:d}", get_state_prefix(), interruptNumber);
    // Only trigger if there was no interrupt before. Is this correct?
    bool wasInterrupt = interruptFlagRegister & 0x7;
    interruptFlagRegister = interruptFlagRegister | interruptNumber;
    // Check if any of the three enable bits is enabled
    uint8_t interrupt = interruptEnableRegister & interruptFlagRegister & 0x7;
    if (!wasInterrupt && interrupt) {
        LOGL_DSC(LogLevel::DEBUG, "{:s} Issuing INT{:d}", get_state_prefix(), interrupt);
        bus->interrupts.notifyAboutInterrupt(INTERRUPT_BIT_CDROM);
    }
}

void CDROM::notifyAboutINT10() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} Notifying about and issuing INT10", get_state_prefix());
    // Only trigger if there was no interrupt before. Is this correct?
    bool wasInterrupt = Bit::getBit(interruptFlagRegister, CDROM_INTERRUPT_FLAG_CLRBFWRDY);
    Bit::setBit(interruptFlagRegister, CDROM_INTERRUPT_FLAG_CLRBFWRDY);
    if (!wasInterrupt && Bit::getBit(interruptEnableRegister, CDROM_INTERRUPT_ENABLE_BFWRDY)) {
        LOGL_DSC(LogLevel::DEBUG, "{:s} Issuing INT10", get_state_prefix());
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

    LOGL_DSC(LogLevel::TRACE, "{:s} 0x{:02X} -> @0x{:08X} with index {:d}", get_state_prefix(), value, address, getIndex());

    if (address == 0x1F801800) { // status register
        LOGL_DSC(LogLevel::TRACE, "{:s} 0x{:02X} -> status register", get_state_prefix(), value);
        // Only the index can be written to
        statusRegister = (statusRegister & 0xF8) | value & 0x3;

    } else if (address == 0x1F801801) {
        switch (getIndex()) {
            case 0: // Command Register
                LOGL_DSC(LogLevel::TRACE, "{:s} @0x{:08X}: 0x{:02X} -> command register", get_state_prefix(), bus->cpu.instructionPC, value);
                command = value;
                pending_command = true;
                send_command();
                break;
            case 1: // Sound Map Data Out
                LOGL_DSC(LogLevel::WARNING, "Unimplemented write to Sound Map Data Out: 0x{:02X} -> @0x{:08X} with index {:d}", value, address, getIndex());
                // TODO Implement
                break;
            case 2: // Sound Map Coding Info
                LOGL_DSC(LogLevel::WARNING, "Unimplemented write to Sound Map Coding Info: 0x{:02X} -> @0x{:08X} with index {:d}", value, address, getIndex());
                // TODO Implement
                break;
            case 3: // Audio Volume for Right-CD-Out to Right-SPU-Input
                LOGL_DSC(LogLevel::TRACE, "0x{:02X} -> Audio Volume for Right-CD-Out to Right-SPU-Input", value);
                audioVolumeCDOutToSPUIn[3] = value;
                break;
            default:
                assert(false);
                break;
        }
    } else if (address == 0x1F801802) {
        switch (getIndex()) {
            case 0: // Parameter Queue
                LOGL_DSC(LogLevel::TRACE, "{:s} 0x{:02X} -> parameter queue", get_state_prefix(), value);
                parameter_queue.push(value);
                break;
            case 1: // Interrupt Enable Register
                LOGL_DSC(LogLevel::TRACE, "{:s} 0x{:02X} -> interrupt enable register", get_state_prefix(), value);
                interruptEnableRegister = value & 0x1F; // Bit 7 to 5 should be zero. Only use 4 to 0.
                // Should we check and issue pending interrupts here?
                break;
            case 2: // Audio Volume for Left-CD-Out to Left-SPU-Input
                LOGL_DSC(LogLevel::TRACE, "0x{:02X} -> Audio Volume for Left-CD-Out to Left-SPU-Input", value);
                audioVolumeCDOutToSPUIn[0] = value;
                break;
            case 3: // Audio Volume for Right-CD-Out to Left-SPU-Input
                LOGL_DSC(LogLevel::TRACE, "0x{:02X} -> Audio Volume for Right-CD-Out to Left-SPU-Input", value);
                audioVolumeCDOutToSPUIn[2] = value;
                break;
            default:
                assert(false);
                break;
        }
    } else if (address == 0x1F801803) {
        switch (getIndex()) {
            case 0: // Request Register
                LOGL_DSC(LogLevel::TRACE, "{:s} 0x{:02X} -> request register", get_state_prefix(), value);
                requestRegister = value;
                if (Bit::getBit(value, CDROM_REQUEST_BFRD)) {
                    LOGL_DSC(LogLevel::DEBUG, "{:s} Serving data queue", get_state_prefix(), value);
                    if (cd) {
                        if (!read_sector_buffers.empty()) {
                            if (current_sector_buffer) {
                                unused_sector_buffers.emplace_back(std::move(current_sector_buffer));
                            }

                            current_sector_buffer = std::move(read_sector_buffers.front());
                            read_sector_buffers.pop_front();

                            bool large_sector_size = mode & (1U << CDROM_MODE_SECTOR_SIZE);
                            sector_offset = large_sector_size ? CD_MODE2_SYNC_BYTES : CD_MODE2_DATA_OFFSET;
                            sector_end = large_sector_size ? CD::SECTOR_SIZE : (CD::SECTOR_SIZE - 0x118);
                        } else {
                            LOGL_DSC(LogLevel::WARNING, "{:s} No ready sector was read, cannot serve!", get_state_prefix());
                        }
                    }
                } else {
                    LOGL_DSC(LogLevel::DEBUG, "{:s} Resetting data queue", get_state_prefix(), value);
                    sector_offset = 0;
                    sector_end = 0;
                }
                break;
            case 1: // Interrupt Flag Register
                LOGL_DSC(LogLevel::TRACE, "{:s} 0x{:02X} -> interrupt flag register", get_state_prefix(), value);
                updateInterruptFlagRegister(value);
                break;
            case 2: // Audio Volume for Left-CD-Out to Right-SPU-Input
                LOGL_DSC(LogLevel::TRACE, "0x{:02X} -> Audio Volume for Left-CD-Out to Right-SPU-Input", value);
                audioVolumeCDOutToSPUIn[1] = value;
                break;
            case 3: // Audio Volume Apply Changes
                LOGL_DSC(LogLevel::WARNING, "Unimplemented write to Audio Volume Apply Changes: 0x{:02X}", value);
                // TODO Implement
                break;
            default:
                assert(false);
                break;
        }

    } else {
        LOGL_DSC(LogLevel::WARNING, "Unimplemented write 0x{:02X} -> @0x{:08X} with index {:d}", value, address, getIndex());
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
        LOGL_DSC(LogLevel::TRACE, "{:s} status register -> 0x{:02X}", get_state_prefix(), value);

    } else if (address == 0x1F801801) {
        switch (getIndex()) {
            case 0: // Mirror of response queue
            case 1: // Response queue
            case 2: // Mirror of response queue
            case 3: // Mirror of response queue
                if (response_queue.is_empty()) {
                    LOGL_DSC(LogLevel::WARNING, "Read from empty response queue!");
                    value = 0;

                } else {
                    value = response_queue.pop();
                }
                // TODO Implement wrap-around of response queue

                LOGL_DSC(LogLevel::TRACE, "{:s} @0x{:08X}: response queue -> 0x{:02X}", get_state_prefix(), bus->cpu.instructionPC, value);
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
                    LOGL_DSC(LogLevel::WARNING, "Read from empty data queue!");
                }
                LOGL_DSC(LogLevel::TRACE, "data queue -> 0x{:02X}", value);
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
                LOGL_DSC(LogLevel::TRACE, "{:s} interrupt enable register -> 0x{:02X}", get_state_prefix(), value);
                break;
            case 1: // Interrupt Flag Register
            case 3: // Mirror of Interrupt Flag Register
                value = interruptFlagRegister | 0xE0; // Bits 7 to 5 are always 1
                LOGL_DSC(LogLevel::TRACE, "{:s} interrupt flag register -> 0x{:02X}", get_state_prefix(), value);
                break;
            default:
                assert(false);
                break;
        }

    } else {
        LOGL_DSC(LogLevel::WARNING, "Unimplemented read @0x{:08X} with index {:d}", address, getIndex());
    }

    LOGL_DSC(LogLevel::TRACE, "{:s} @0x{:08X} with index {:d} -> 0x{:02X}", get_state_prefix(), address, getIndex(), value);

    return value;
}

uint8_t CDROM::getIndex() const {
    return statusRegister & 0x3;
}

void CDROM::updateStatusRegister() {
    Bit::setBit(statusRegister, CDROM_STATUS_BUSYSTS, pending_command);
    Bit::setBit(statusRegister, CDROM_STATUS_DRQSTS, has_data());
    Bit::setBit(statusRegister, CDROM_STATUS_RSLRRDY, !response_queue.is_empty());
    Bit::setBit(statusRegister, CDROM_STATUS_PRMWRDY, !parameter_queue.is_full());
    Bit::setBit(statusRegister, CDROM_STATUS_PRMEMPT, parameter_queue.is_empty());
    Bit::setBit(statusRegister, CDROM_STATUS_ADPBUSY, 0); // TODO XA-ADPCM
}

void CDROM::updateInterruptFlagRegister(uint8_t value) {
    //LOG_CDROM("{:s} Updating flag register", get_state_prefix());
    // 7 - CHPRST: Unknown

    // 6 - CLRPRM: Reset Parameter Queue
    if (Bit::getBit(value, CDROM_INTERRUPT_FLAG_CLRPRM)) {
        LOGL_DSC(LogLevel::DEBUG, "{:s} Resetting parameter queue", get_state_prefix());
        parameter_queue.clear();
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
    //LOG_CDROM("Was interrupt active before: {:s}, is interrupt active now: {:s}", wasInterrupt, isInterrupt);

    // Acknowledge empties response queue, sends pending command (if there is one)
    // But what counts as an "acknowledge"?
    // Would clearing a single bit of, say, INT3 suffice?
    // And what about INT10 and INT8?
    // Let's assume that we only consider INT1...7 and that it has to be cleared completely
    if (wasInterrupt && !isInterrupt) {
        LOGL_DSC(LogLevel::TRACE, "{:s} Acknowledged interrupt: pending_command = {:s}", get_state_prefix(), pending_command);
        waiting_for_acknowledge = false;

        // Clear response queue
        // TODO Do we have to clear this or is the user responsible for this?
        response_queue.clear();

        // Check if there is a pending command
        // Note that pending_command determines/corresponds to CDROM_STATUS_BUSYSTS
        // (If we have a pending command, we want to send it now and clear CDROM_STATUS_BUSYSTS)
        if (pending_command) {
            send_command();
        }
    }
}

bool CDROM::has_data() {
    return sector_offset < sector_end;
}

uint8_t CDROM::read_byte() {
    return current_sector_buffer[sector_offset++];
}

uint32_t CDROM::read_word() {
    uint32_t value = *(reinterpret_cast<const uint32_t*>(&current_sector_buffer[sector_offset]));
    sector_offset += 4;
    return value;
}

const CDROM::Command CDROM::commands[] = {
    // 0x00
    &CDROM::unknown,
    &CDROM::get_stat, // 0x01
    &CDROM::set_loc, // 0x02
    &CDROM::play, // 0x03
    &CDROM::unknown, &CDROM::unknown,
    &CDROM::read_n_s,
    &CDROM::unknown,
    &CDROM::stop, // 0x08
    &CDROM::pause, // 0x09
    &CDROM::init, // 0x0A
    &CDROM::mute, // 0x0B
    &CDROM::demute, // 0x0C
    &CDROM::set_filter, // 0x0D
    &CDROM::set_mode, //0x0E
    &CDROM::unknown,
    // 0x10
    &CDROM::get_loc_l, // 0x10
    &CDROM::get_loc_p, // 0x11
    &CDROM::unknown,
    &CDROM::get_tn, // 0x13
    &CDROM::get_td, // 0x14
    &CDROM::seek_l, // 0x15
    &CDROM::seek_p, // 0x16
    &CDROM::unknown,
    &CDROM::unknown,
    &CDROM::test, // 0x19
    &CDROM::get_id, // 0x1A
    &CDROM::read_n_s, // 0x1B
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

const CDROM::Command CDROM::sub_functions[] = {
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

void CDROM::get_stat() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Getstat(): Command <========", get_state_prefix());
    scheduled_responses.emplace_back(&CDROM::get_stat_response);
}

uint8_t CDROM::get_stat_response() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Getstat(): Response <========", get_state_prefix());
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

void CDROM::set_loc() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Setloc(): Command <========", get_state_prefix());
    scheduled_responses.emplace_back(&CDROM::set_loc_response);
}

uint8_t CDROM::set_loc_response() {
    amm = parameter_queue.pop();
    ass = parameter_queue.pop();
    asect = parameter_queue.pop();
    set_loc_unprocessed = true;

    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Setloc(0x{:02X}, 0x{:02X}, 0x{:02X}): Response <========", get_state_prefix(), amm, ass, asect);

    if (!cd) {
        return no_disc_response();
    }

    response_queue.push(0x02);
    return 3;
}

void CDROM::play() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Play(): Command <========", get_state_prefix());
    scheduled_responses.emplace_back(&CDROM::play_response);

    // Make sure we do not have any already read sectors remaining
    while (!read_sector_buffers.empty()) {
        unused_sector_buffers.emplace_back(std::move(read_sector_buffers.back()));
        read_sector_buffers.pop_back();
    }
}

uint8_t CDROM::play_response() {
    uint8_t track = !parameter_queue.is_empty() ? parameter_queue.pop() : 0;
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Play({:d}): Initial Response <========", get_state_prefix(), track);

    if (!cd) {
        return no_disc_response();
    }

    if (track != 0) { // Track given -> start at that
        cd->seek_to_audio_track(track);
        // TODO Support restart current track
    } else if (set_loc_unprocessed) { // No track given but SetLoc used - > use that
        cd->seek_to_bcd(amm, ass, asect);
        set_loc_unprocessed = false;
    } // Otherwise -> use current position

    // Write response
    drive_state = PLAYING;
    push_drive_state_to_response_queue();

    scheduled_responses.emplace_back(&CDROM::play_second_response, Bit::getBit(mode, CDROM_MODE_SPEED) ? 0x36CD2 : 0x6E1CD);

    return 3;
}

uint8_t CDROM::play_second_response() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Play(): Second Response <========", get_state_prefix());

    // Read sector from CD
    uint8_t buffer[CD::SECTOR_SIZE];
    //std::unique_ptr<uint8_t[]> buffer;
    //if (!unused_sector_buffers.empty()) {
    //    buffer = std::move(unused_sector_buffers.front());
    //    unused_sector_buffers.pop_front();

    //} else {
    //    LOGL_DSC(LogLevel::WARNING, "{:s} No unused sector buffer for read, allocating new buffer!", get_state_prefix());
    //    buffer = std::make_unique<uint8_t[]>(CD::SECTOR_SIZE);
    //}

    LOGL_DSC(LogLevel::DEBUG, "{:s} CD is at {}", get_state_prefix(), cd->get_current_position_on_disc());
    cd->read_sector_and_advance(buffer);
    //read_sector_buffers.emplace_back(std::move(buffer));

    bus->spu.play_cd_da_sector(buffer);

    // Schedule reading of next sector (since we are automatically reading that)
    scheduled_responses.emplace_back(&CDROM::play_second_response, Bit::getBit(mode, CDROM_MODE_SPEED) ? 0x36CD2 : 0x6E1CD);

    if (!Bit::getBit(mode, CDROM_MODE_REPORT)) {
        return 0;
    }

    // (Possibly) report
    auto disc_pos = cd->get_current_position_on_disc();
    uint32_t amm = (disc_pos.minutes / 10) * 0x10 + (disc_pos.minutes % 10);
    uint32_t ass = (disc_pos.seconds / 10) * 0x10 + (disc_pos.seconds % 10);
    uint32_t asect = (disc_pos.sectors / 10) * 0x10 + (disc_pos.sectors % 10);

    if (asect == 0x00 || asect == 0x20 || asect == 0x40 || asect == 0x60) {
        // Report absolute time
        push_drive_state_to_response_queue(); // stat
        response_queue.push(cd->at_end_of_disc() ? 0xAA : cd->get_current_track_number());
        response_queue.push(cd->get_current_index_number());
        response_queue.push(amm);
        response_queue.push(ass);
        response_queue.push(asect);
        response_queue.push(0x1F); //TODO peaklo
        response_queue.push(0x00); //TODO peakhi
        return 1;

    } else if (asect == 0x10 || asect == 0x30 || asect == 0x50 || asect == 0x70) {
        // Report time within current track
        auto track_pos = cd->get_current_position_in_track();

        push_drive_state_to_response_queue(); // stat
        response_queue.push(cd->at_end_of_disc() ? 0xAA : cd->get_current_track_number());
        response_queue.push(cd->get_current_index_number());
        response_queue.push((track_pos.minutes / 10) * 0x10 + (track_pos.minutes % 10));
        response_queue.push((track_pos.seconds / 10) * 0x10 + (track_pos.seconds % 10) + 0x80);
        response_queue.push((track_pos.sectors / 10) * 0x10 + (track_pos.sectors % 10));
        response_queue.push(0x1F); //TODO peaklo
        response_queue.push(0x00); //TODO peakhi
        return 1;

    } else {
        return 0; // Do not report after all
    }
}


void CDROM::read_n_s() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> ReadN/S(): Command <========", get_state_prefix());
    scheduled_responses.emplace_back(&CDROM::read_n_s_response);

    // Make sure we do not have any already read sectors remaining
    while (!read_sector_buffers.empty()) {
        unused_sector_buffers.emplace_back(std::move(read_sector_buffers.back()));
        read_sector_buffers.pop_back();
    }
}

uint8_t CDROM::read_n_s_response() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> ReadN/S(): Initial Response <========", get_state_prefix());

    if (!cd) {
        return no_disc_response();
    }

    // Write response
    drive_state = READING;
    push_drive_state_to_response_queue();

    // Read does also seek
    // TODO Move this to second response, separate spam response
    cd->seek_to_bcd(amm, ass, asect);
    set_loc_unprocessed = false;

    // Schedule second response
    scheduled_responses.emplace_back(&CDROM::read_n_s_second_response, 0x36CD2);

    return 3;
}

uint8_t CDROM::read_n_s_second_response() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> ReadN/S(): Second Response <========", get_state_prefix());

    // Read sector from CD
    std::unique_ptr<uint8_t[]> buffer;
    if (!unused_sector_buffers.empty()) {
        buffer = std::move(unused_sector_buffers.front());
        unused_sector_buffers.pop_front();

    } else {
        LOGL_DSC(LogLevel::WARNING, "{:s} No unused sector buffer for read, allocating new buffer!", get_state_prefix());
        buffer = std::make_unique<uint8_t[]>(CD::SECTOR_SIZE);
    }

    LOGL_DSC(LogLevel::DEBUG, "{:s} CD is at {}", get_state_prefix(), cd->get_current_position_on_disc());
    last_sector_header.was_data = cd->get_current_track_mode() != util::cue::Track::Mode::AUDIO;
    cd->read_sector_and_advance(buffer.get());
    if (last_sector_header.was_data) {
        last_sector_header.header[0] = buffer[0x00C];
        last_sector_header.header[1] = buffer[0x00D];
        last_sector_header.header[2] = buffer[0x00E];
        last_sector_header.header[3] = buffer[0x00F];
        last_sector_header.sub_header[0] = buffer[0x010];
        last_sector_header.sub_header[1] = buffer[0x011];
        last_sector_header.sub_header[2] = buffer[0x012];
        last_sector_header.sub_header[3] = buffer[0x013];
        LOGL_DSC(LogLevel::DEBUG, "{:s} Extracted (sub-)header: 0x{:02X}, 0x{:02X}, 0x{:02X}, 0x{:02X}, 0x{:02X}, 0x{:02X}, 0x{:02X}, 0x{:02X}", get_state_prefix(),
                        last_sector_header.header[0], last_sector_header.header[1], last_sector_header.header[2], last_sector_header.header[3],
                        last_sector_header.sub_header[0], last_sector_header.sub_header[1], last_sector_header.sub_header[2], last_sector_header.sub_header[3]);

        uint8_t file = last_sector_header.sub_header[0];
        uint8_t channel = last_sector_header.sub_header[1];
        uint8_t submode = last_sector_header.sub_header[2];
        LOGL_DSC(LogLevel::DEBUG, "{:s} File: 0x{:02X}, Channel: 0x{:02X}, Submode: 0x{:02X}", get_state_prefix(), file, channel, submode);

        bool is_mode_2 = last_sector_header.header[3] == 0x02;
        bool is_audio_and_realtime = (submode & 0x44) == 0x44; // Bit 2 and 6 set
        if (is_mode_2 && is_audio_and_realtime && Bit::getBit(mode, CDROM_MODE_XA_ADPCM)) {
        LOGL_DSC(LogLevel::DEBUG, "{:s} XA-ADPCM enabled and encountered XA-ADPCM sector", get_state_prefix());
            if (!Bit::getBit(mode, CDROM_MODE_XA_FILTER) || (file == filter_file && channel == filter_channel)) {
                // TODO Deliver XA-ADPCM sector
                LOGL_DSC(LogLevel::WARNING, "{:s} XA-ADPCM playback not implemented!", get_state_prefix());

            } else {
                LOGL_DSC(LogLevel::DEBUG, "{:s} Skipping XA-ADPCM sector due to filter", get_state_prefix());
            }
            // Do not send sector to CPU, just move buffer back
            unused_sector_buffers.emplace_back(std::move(buffer));

            // Schedule next read
            scheduled_responses.emplace_back(&CDROM::read_n_s_second_response, 0x36CD2);

            return 0;
        }
    }

    read_sector_buffers.emplace_back(std::move(buffer));

    push_drive_state_to_response_queue();

    // Schedule reading of next sector (since we are automatically reading that)
    // Software has to be fast enough to keep up!
    // That is, the next_sector_buffer has to be read or we will overwrite it.
    scheduled_responses.emplace_back(&CDROM::read_n_s_second_response, 0x36CD2);

    return 1;
}

void CDROM::stop() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Stop(): Command <========", get_state_prefix());
    scheduled_responses.clear();
    scheduled_responses.emplace_back(&CDROM::stop_response);
}

uint8_t CDROM::stop_response() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Stop(): Initial Response <========", get_state_prefix());

    if (!cd) {
        return no_disc_response();
    }

    // Schedule second response
    scheduled_responses.emplace_back(&CDROM::stop_second_response);

    push_drive_state_to_response_queue(); // Respond with current state
    return 3;
}

uint8_t CDROM::stop_second_response() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Stop(): Second Response <========", get_state_prefix());

    drive_state = MOTOR_OFF;
    push_drive_state_to_response_queue();
    return 2;
}

void CDROM::pause() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Pause(): Command <========", get_state_prefix());
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Pause(): Clearing Response Queue <========", get_state_prefix(), command);
    scheduled_responses.clear();
    scheduled_responses.emplace_back(&CDROM::pause_response);
}

uint8_t CDROM::pause_response() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Pause(): Initial Response <========", get_state_prefix());

    if (!cd) {
        return no_disc_response();
    }

    // Schedule second response
    scheduled_responses.emplace_back(&CDROM::pause_second_response, 0x10BD93);

    push_drive_state_to_response_queue(); // Respond with current state
    return 3;
}

uint8_t CDROM::pause_second_response() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Pause(): Second Response <========", get_state_prefix());

    drive_state = MOTOR_ON;
    push_drive_state_to_response_queue();
    return 2;
}

void CDROM::init() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Init(): Command <========", get_state_prefix());
    scheduled_responses.emplace_back(&CDROM::init_response, 0x13CCE);
}

uint8_t CDROM::init_response() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Init(): Initial Response <========", get_state_prefix());

    // TODO set mode to 0x20
    scheduled_responses.emplace_back(&CDROM::init_second_response);

    push_drive_state_to_response_queue(); // Current/old state
    return 3;
}

uint8_t CDROM::init_second_response() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Init(): Second Response <========", get_state_prefix());

    drive_state = MOTOR_ON;
    push_drive_state_to_response_queue();
    return 2;
}

void CDROM::mute() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Mute(): Command <========", get_state_prefix());
    scheduled_responses.emplace_back(&CDROM::mute_response);
}

uint8_t CDROM::mute_response() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Mute(): Response <========", get_state_prefix());

    // TODO Do something?
    LOGL_DSC(LogLevel::WARNING, "{:s} Mute() not implemented!", get_state_prefix());

    push_drive_state_to_response_queue();
    return 3;
}

void CDROM::demute() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Demute(): Command <========", get_state_prefix());
    scheduled_responses.emplace_back(&CDROM::demute_response);
}

uint8_t CDROM::demute_response() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Demute(): Response <========", get_state_prefix());

    // TODO Do something?
    LOGL_DSC(LogLevel::WARNING, "{:s} Demute() not implemented!", get_state_prefix());

    push_drive_state_to_response_queue();
    return 3;
}

void CDROM::set_filter() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> SetFilter(): Command <========", get_state_prefix());
    scheduled_responses.emplace_back(&CDROM::set_filter_response);
}

uint8_t CDROM::set_filter_response() {
    filter_file = parameter_queue.pop();
    filter_channel = parameter_queue.pop();
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> SetFilter(0x{:02X}, 0x{:02X}): Response <========", get_state_prefix(), filter_file, filter_channel);

    push_drive_state_to_response_queue();
    return 3;
}

void CDROM::set_mode() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Setmode(): Command <========", get_state_prefix());
    scheduled_responses.emplace_back(&CDROM::set_mode_response);
}

uint8_t CDROM::set_mode_response() {
    mode = parameter_queue.pop();
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Setmode(0x{:02X}): Response <========", get_state_prefix(), mode);

    if (Bit::getBit(mode, CDROM_MODE_IGNORE)) {
        LOGL_DSC(LogLevel::WARNING, "{:s} Ignore bit set but not implemented", get_state_prefix(), mode);
    }

    // TODO Handle speed, auto pause and cdda

    push_drive_state_to_response_queue();
    return 3;
}

void CDROM::get_loc_l() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> GetLocL(): Command <========", get_state_prefix());
    scheduled_responses.emplace_back(&CDROM::get_loc_l_response);
}

uint8_t CDROM::get_loc_l_response() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> GetLocL(): Response <========", get_state_prefix());

    if (!cd || !last_sector_header.was_data) { // TODO Correct response for was not data
        return no_disc_response();
    }


    auto disc_pos = cd->get_current_position_on_disc();

    response_queue.push(last_sector_header.header[0]); // amm
    response_queue.push(last_sector_header.header[1]); // ass
    response_queue.push(last_sector_header.header[2]); // asect
    response_queue.push(last_sector_header.header[3]); // mode
    response_queue.push(last_sector_header.sub_header[0]); // file
    response_queue.push(last_sector_header.sub_header[1]); // channel
    response_queue.push(last_sector_header.sub_header[2]); // sm
    response_queue.push(last_sector_header.sub_header[3]); // ci
    return 3;
}

void CDROM::get_loc_p() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> GetLocP(): Command <========", get_state_prefix());
    scheduled_responses.emplace_back(&CDROM::get_loc_p_response);
}

uint8_t CDROM::get_loc_p_response() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> GetLocP(): Response <========", get_state_prefix());

    if (!cd) {
        return no_disc_response();
    }

    auto disc_pos = cd->get_current_position_on_disc();
    auto track_pos = cd->get_current_position_in_track();

    uint8_t response[8];
    response[0] = cd->at_end_of_disc() ? 0xAA : cd->get_current_track_number();
    response[1] = cd->get_current_index_number();
    response[2] = (track_pos.minutes / 10) * 0x10 + (track_pos.minutes % 10);
    response[3] = (track_pos.seconds / 10) * 0x10 + (track_pos.seconds % 10);
    response[4] = (track_pos.sectors / 10) * 0x10 + (track_pos.sectors % 10);
    response[5] = (disc_pos.minutes / 10) * 0x10 + (disc_pos.minutes % 10);
    response[6] = (disc_pos.seconds / 10) * 0x10 + (disc_pos.seconds % 10);
    response[7] = (disc_pos.sectors / 10) * 0x10 + (disc_pos.sectors % 10);

    for (uint32_t i = 0; i < 8; ++i) {
        LOGL_DSC(LogLevel::DEBUG, "{:s} 0x{:02X}", get_state_prefix(), response[i]);
        response_queue.push(response[i]);
    }

    return 3;
}

void CDROM::get_tn() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> GetTN(): Command <========", get_state_prefix());
    scheduled_responses.emplace_back(&CDROM::get_tn_response);
}

uint8_t CDROM::get_tn_response() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> GetTN(): Response <========", get_state_prefix());

    if (!cd) {
        return no_disc_response();
    }

    push_drive_state_to_response_queue();
    response_queue.push(cd->get_first_track_number());
    response_queue.push(cd->get_last_track_number());
    return 3;
}

void CDROM::get_td() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> GetTD(): Command <========", get_state_prefix());
    scheduled_responses.emplace_back(&CDROM::get_td_response);
}

uint8_t CDROM::get_td_response() {
    uint8_t track = parameter_queue.pop();
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> GetTD(0x{:02X}): Response <========", get_state_prefix(), track);

    if (!cd) {
        return no_disc_response();
    }

    auto track_pos = cd->get_index_one_of_track(track);
    push_drive_state_to_response_queue();
    response_queue.push((track_pos.minutes / 10) * 0x10 + (track_pos.minutes % 10));
    response_queue.push((track_pos.seconds / 10) * 0x10 + (track_pos.seconds % 10));
    return 3;
}

void CDROM::seek_l() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> SeekL(): Command <========", get_state_prefix());
    scheduled_responses.emplace_back(&CDROM::seek_l_response);
}

uint8_t CDROM::seek_l_response() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> SeekL(): Initial Response <========", get_state_prefix());

    if (!cd) {
        return no_disc_response();
    }

    scheduled_responses.emplace_back(&CDROM::seek_l_second_response);

    drive_state = SEEKING;
    push_drive_state_to_response_queue();
    return 3;
}

uint8_t CDROM::seek_l_second_response() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> SeekL(): Second Response <========", get_state_prefix());

    cd->seek_to_bcd(amm, ass, asect);
    set_loc_unprocessed = false;

    drive_state = MOTOR_ON;
    push_drive_state_to_response_queue();
    return 2;
}

void CDROM::seek_p() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> SeekP(): Command <========", get_state_prefix());
    scheduled_responses.clear(); // Playing is stopped
    scheduled_responses.emplace_back(&CDROM::seek_p_response);
}

uint8_t CDROM::seek_p_response() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> SeekP(): Initial Response <========", get_state_prefix());

    if (!cd) {
        return no_disc_response();
    }

    scheduled_responses.emplace_back(&CDROM::seek_p_second_response);

    drive_state = SEEKING;
    push_drive_state_to_response_queue();
    return 3;
}

uint8_t CDROM::seek_p_second_response() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> SeekP(): Second Response <========", get_state_prefix());

    cd->seek_to_bcd(amm, ass, asect);
    set_loc_unprocessed = false;

    drive_state = MOTOR_ON;
    push_drive_state_to_response_queue();
    return 2;
}
void CDROM::test() {
    function = parameter_queue.pop();
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Test(0x{:02X}): Command <========", get_state_prefix(), function);
    (this->*sub_functions[function])();
}

void CDROM::get_id() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> GetID(): Command <========", get_state_prefix());
    scheduled_responses.emplace_back(&CDROM::get_id_response);
}

uint8_t CDROM::get_id_response() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> GetID(): Initial Response <========", get_state_prefix());

    if (drive_state == OPEN) {
        response_queue.push(0x11);
        response_queue.push(0x80);
        return 5;
    }

    if (!cd) {
        scheduled_responses.emplace_back(&CDROM::get_id_second_response_motor_off);
        response_queue.push(0x00);
        return 3;
    }

    if (drive_state == MOTOR_OFF) {
        scheduled_responses.emplace_back(&CDROM::get_id_second_response_motor_off);
        response_queue.push(0x00);
        return 3;

    }

    if (cd->is_audio_cd()) {
        scheduled_responses.emplace_back(&CDROM::get_id_second_response_audio);
    } else {
        // Licensed Mode 2
        scheduled_responses.emplace_back(&CDROM::get_id_second_response_mode_2);
    }

    response_queue.push(0x02);
    return 3;
}

uint8_t CDROM::get_id_second_response_motor_off() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> GetID(): Second Response (Motor Off) <========", get_state_prefix());

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

uint8_t CDROM::get_id_second_response_mode_2() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> GetID(): Second Response (Licensed Disc, Mode 2) <========", get_state_prefix());

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

uint8_t CDROM::get_id_second_response_audio() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> GetID(): Second Response (Audio Disc) <========", get_state_prefix());

    response_queue.push(0x0A); // stat
    response_queue.push(0x90); // flags
    response_queue.push(0x00); // type
    response_queue.push(0x00); // atip
    response_queue.push(0x00); // S
    response_queue.push(0x00); // C
    response_queue.push(0x00); // E
    response_queue.push(0x00); // A
    return 5;
}

void CDROM::read_toc() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> ReadTOC(): Command <========", get_state_prefix());
    scheduled_responses.emplace_back(&CDROM::read_toc_response);
}

uint8_t CDROM::read_toc_response() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> ReadTOC(): Initial Response <========", get_state_prefix());
    // INT3 with status first, then INT2 with status

    scheduled_responses.emplace_back(&CDROM::read_toc_second_response, 0x1F78A40);
    push_drive_state_to_response_queue();
    return 3;
}

uint8_t CDROM::read_toc_second_response() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> ReadTOC(): Second Response <========", get_state_prefix());

    push_drive_state_to_response_queue();
    return 2;
}

void CDROM::unknown_sf() {
    throw exceptions::UnknownCDROMFunctionError(std::format("Unknown function 0x{:02X}", function));
}

void CDROM::function_0x20() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Function 0x20: Command <========", get_state_prefix());
    scheduled_responses.emplace_back(&CDROM::function_0x20_response);
}

uint8_t CDROM::function_0x20_response() {
    LOGL_DSC(LogLevel::DEBUG, "{:s} ========> Function 0x20: Response <========", get_state_prefix());

    // hard-coded answer
    response_queue.push(0x99); // Year
    response_queue.push(0x02); // Month
    response_queue.push(0x01); // Day
    response_queue.push(0xC3); // Version
    return 3;
}

}

