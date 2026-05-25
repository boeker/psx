#ifndef PSX_CDROM_H
#define PSX_CDROM_H

#include <cstdint>
#include <deque>
#include <memory>
#include <ostream>
#include <span>

namespace PSX {

// 0x1F801800 - Status Register (read only with the exception of bits 1 and 0)
#define CDROM_STATUS_BUSYSTS 7 // Command/parameter transmission busy (1 = busy)
#define CDROM_STATUS_DRQSTS 6 // Data queue non-empty (1 = non-empty)
#define CDROM_STATUS_RSLRRDY 5 // Response queue non-empty (1 = non-empty)
#define CDROM_STATUS_PRMWRDY 4 // Parameter queue non-full (1 = non-full)
#define CDROM_STATUS_PRMEMPT 3 // Parameter queue empty (1 = empty)
#define CDROM_STATUS_ADPBUSY 2 // XA=ADPCM queue empty (0 = empty), i.e., not playing XA-ADPCM sound
#define CDROM_STATUS_INDEX1 1 // Bit 1 of index (0...3 = index 0...3) (writable)
#define CDROM_STATUS_INDEX0 0 // Bit 0 of index (writable)

// 0x1F801802 and 0x1F801803 - Interrupt Enable Register
// 0x1F801802, index 1 write
// 0x1F801803, index 0 read
// 0x1F801803, index 2 read
#define CDROM_INTERRUPT_ENABLE_BFWRDY 4 // INT10
#define CDROM_INTERRUPT_ENABLE_BFEMPT 3 // INT8
#define CDROM_INTERRUPT_ENABLE_EN2 2 // INT1...7 (encoded in binary)
#define CDROM_INTERRUPT_ENABLE_EN1 1 // This is
#define CDROM_INTERRUPT_ENABLE_EN0 0 // really weird

// 0x1F801803 - Interrupt Flag Register
// 0x1F801803, index 1 write
// 0x1F801803, index 1 read
// 0x1F801803, index 3 read
#define CDROM_INTERRUPT_FLAG_CHPRST 7 // Write 1: unknown, read: always 1
#define CDROM_INTERRUPT_FLAG_CLRPRM 6 // Write 1: reset parameter queue, read: always 1
#define CDROM_INTERRUPT_FLAG_SMADPCLR 5 // Write 1: unknown (clear sound map out?), read: always 1
#define CDROM_INTERRUPT_FLAG_CLRBFWRDY 4 // Write 1: acknowledge INT10, read: command start (INT10)
#define CDROM_INTERRUPT_FLAG_CLRBFEMPT 3 // Write 1: acknowledge INT8 read: unknown (usually 0)
#define CDROM_INTERRUPT_FLAG_ACK2 2 // Write: acknowledge INT1...7 (encoded in binary)
#define CDROM_INTERRUPT_FLAG_ACK1 1 // Read: Response receivied (INT1...7)
#define CDROM_INTERRUPT_FLAG_ACK0 0

// 0x1F801803 - Request Register
// 0x1F801803, index 0 write
#define CDROM_REQUEST_BFRD 7 // Want data (0 = no, reset data queue, 1 = yes, load data queue)
#define CDROM_REQUEST_BFWR 6 // Unknown
#define CDROM_REQUEST_SMEN 5 // Start interrupt (INT10) on next command

#define CDROM_MODE_SECTOR_SIZE 5 // Sector Size (0 = 0x800, data only, 1 = 0x924, whole sector except sync bytes)

class Queue {
private:
    uint8_t queue[16];
    uint8_t in;
    uint8_t out;
    uint8_t elements;

    friend std::ostream& operator<<(std::ostream &os, const Queue &queue);

public:
    Queue();
    void clear();
    void push(uint8_t parameter);
    uint8_t pop();
    bool isEmpty();
    bool isFull();
};

class Bus;
class CD;

class CDROM {
private:
    Bus *bus;

    uint8_t statusRegister;
    uint8_t audioVolumeCDOutToSPUIn[4]; // Left -> Left, Left -> Right, Right -> Left, Right -> Right
    uint8_t interruptEnableRegister;
    uint8_t interruptFlagRegister;
    uint8_t requestRegister;

    enum DriveState {
        STAY,
        OPEN,
        MOTOR_OFF,
        MOTOR_ON,
        PLAYING,
        SEEKING,
        READING
    };
    static std::string driveStateToString(DriveState driveState);
    static uint8_t driveStateToStatByte(DriveState driveState);
    DriveState drive_state;

    std::string prependState(const std::string &str) const;

    uint8_t command;
    bool pending_command;
    uint8_t function;
    // The PSX's parameter queue containing the command parameter bytes
    Queue parameter_queue;

    std::unique_ptr<CD> cd;

    // The current sector being served
    std::unique_ptr<uint8_t[]> current_sector_buffer;
    // Buffers that already have been read
    std::deque<std::unique_ptr<uint8_t[]>> read_sector_buffers;
    std::deque<std::unique_ptr<uint8_t[]>> unused_sector_buffers;

    uint8_t amm;
    uint8_t ass;
    uint8_t asect;

    uint8_t mode;
    uint32_t sector_offset;
    uint32_t sector_end;

    typedef uint8_t (CDROM::*ResponseFunction) ();
    struct ScheduledResponse {
        ResponseFunction function;
        uint32_t cycles;

        ScheduledResponse(ResponseFunction function, uint32_t cycles)
            : function(function), cycles(cycles) {
        }

        ScheduledResponse(ResponseFunction function)
            : ScheduledResponse(function, 0xC4E1) {
        }
    };

    // The emulated responses from the CDROM controller
    std::deque<ScheduledResponse> scheduled_responses;
    // Number of cycles until next response will be served
    uint32_t cycles_left;
    // The PSX's response queue containing the response bytes
    Queue response_queue;

public:
    CDROM(Bus *bus);
    ~CDROM();
    void reset();
    void setCD(std::unique_ptr<CD> cd);
    CD& getCD();
    void catchUpToCPU(uint32_t cycles);

    void deliver_response(ScheduledResponse &response);
    void send_command();
    void notifyAboutINT1to7(uint8_t interruptNumber);
    void notifyAboutINT10();

    template <typename T>
    void write(uint32_t address, T value);
    template <typename T>
    T read(uint32_t address);

    uint8_t getIndex() const;
    void updateStatusRegister();
    void updateInterruptFlagRegister(uint8_t value);

    bool has_data();
    uint8_t read_byte();
    uint32_t read_word();

private:
    // Command table and implementations
    typedef void (CDROM::*Command) ();

    void push_drive_state_to_response_queue();
    // Generic response when no disc is inserted
    uint8_t no_disc_response();

    static const Command commands[];
    void unknown();
    // 0x01
    void get_stat();
    uint8_t get_stat_response();
    // 0x02
    void set_loc();
    uint8_t set_loc_response();
    // 0x06
    void read_n();
    uint8_t read_n_response();
    uint8_t read_n_second_response();
    // 0x08
    void stop();
    uint8_t stop_response();
    uint8_t stop_second_response();
    // 0x09
    void pause();
    uint8_t pause_response();
    uint8_t pause_second_response();
    // 0x0A
    void init();
    uint8_t init_response();
    uint8_t init_second_response();
    // 0x0C
    void demute();
    uint8_t demute_response();
    // 0x0E
    void set_mode();
    uint8_t set_mode_response();
    // 0x13
    void get_tn();
    uint8_t get_tn_response();
    // 0x14
    void get_td();
    uint8_t get_td_response();
    // 0x15
    void seek_l();
    uint8_t seek_l_response();
    uint8_t seek_l_second_response();
    // 0x19
    void test();
    // 0x1A
    void get_id();
    uint8_t get_id_response();
    uint8_t get_id_second_response_motor_off();
    uint8_t get_id_second_response_mode_2();
    // 0x1E
    void read_toc();
    uint8_t read_toc_response();
    uint8_t read_toc_second_response();

    static const Command sub_functions[];
    void unknown_sf();
    void function_0x20();
    uint8_t function_0x20_response();
};

}

#endif
