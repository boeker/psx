#ifndef PSX_CDROM_H
#define PSX_CDROM_H

#include <cstdint>
#include <deque>
#include <ostream>

namespace PSX {

// 1F801800h - Index/Status Register (Bit0-1 R/W) (Bit2-7 Read Only)
// 7 BUSYSTS Command/parameter transmission busy  (1=Busy)
#define CDROMSTAT_BUSYSTS 7
// 6 DRQSTS  Data fifo empty      (0=Empty) ;triggered after reading LAST byte
#define CDROMSTAT_DRQSTS  6
// 5 RSLRRDY Response fifo empty  (0=Empty) ;triggered after reading LAST byte
#define CDROMSTAT_RSLRRDY 5
// 4 PRMWRDY Parameter fifo full  (0=Full)  ;triggered after writing 16 bytes
#define CDROMSTAT_PRMWRDY 4
// 3 PRMEMPT Parameter fifo empty (1=Empty) ;triggered before writing 1st byte
#define CDROMSTAT_PRMEMPT 3
// 2 ADPBUSY XA-ADPCM fifo empty  (0=Empty) ;set when playing XA-ADPCM sound
#define CDROMSTAT_ADPBUSY 2
// 0-1 Index   Port 1F801801h-1F801803h index (0..3 = Index0..Index3)   (R/W)
#define CDROMSTAT_INDEX1  1
#define CDROMSTAT_INDEX0  0

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

class CDROM {
private:
    Bus *bus;

    // Status Register
    // 0x1F801800
    // index is part of the status register
    uint8_t index;

    // Interrupt Enable Register
    // 0x1F801802, index 1 write
    // 0x1F801803, index 0 read
    // 0x1F801803, index 2 read
    uint8_t interruptEnableRegister;

    // Interrupt Flag Register
    // 0x1F801803, index 1 read and write
    // 0x1F801803, index 3 read
    uint8_t interruptFlagRegister;

    Queue parameterQueue;
    Queue responseQueue;

    typedef void (CDROM::*QueuedResponse) ();
    std::deque<QueuedResponse> queuedResponses;

public:
    CDROM(Bus *bus);
    void reset();

    void executeCommand(uint8_t command);
    void checkAndNotifyINT3();
    void checkAndNotifyINT5();

    uint8_t getStatusRegister();

    template <typename T>
    void write(uint32_t address, T value);

    template <typename T>
    T read(uint32_t address);

    void getIDSecondResponse();
};

}

#endif
