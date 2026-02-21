#ifndef PSX_CDROM_H
#define PSX_CDROM_H

#include <cstdint>
#include <deque>
#include <memory>
#include <ostream>

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

    uint8_t command;
    uint8_t function;
    Queue parameterQueue;
    uint8_t responseInterrupt;
    Queue responseQueue;
    uint8_t secondResponseInterrupt;
    Queue secondResponseQueue;

    std::unique_ptr<CD> cd;

    enum InternalState {
        IDLE,
        TRANSFER_COMMAND,
        PRODUCE_SECOND_RESPONSE
    };
    static std::string internalStateToString(InternalState internalState);
    InternalState internalState;

    enum DriveState {
        OPEN,
        NO_DISC,
        MOTOR_OFF,
        MOTOR_ON,
        SEEKING
    };
    static std::string driveStateToString(DriveState driveState);
    DriveState driveState;

    std::string stateAsString() const;

    uint32_t cyclesLeft;

public:
    CDROM(Bus *bus);
    void reset();

    void catchUpToCPU(uint32_t cycles);

    void setCD(std::unique_ptr<CD> cd);

    void updateStatusRegister();
    uint8_t getIndex() const;

    void sendCommand();
    void notifyAboutINT1to7(uint8_t interruptNumber);
    void notifyAboutINT10();
    void updateInterruptFlagRegister(uint8_t value);

    template <typename T>
    void write(uint32_t address, T value);

    template <typename T>
    T read(uint32_t address);

private:
    // Command table and implementations
    typedef void (CDROM::*Command) ();

    static const Command commands[];
    void Unknown();
    // 0x01
    void Getstat();
    // 0x08
    void Stop();
    // 0x19
    void Test();
    // 0x1A
    void GetID();

    static const Command subFunctions[];
    void UnknownSF();
    void Function0x20();
};

}

#endif
