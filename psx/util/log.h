#ifndef UTIL_LOG_H
#define UTIL_LOG_H

#include <chrono>
#include <ostream>
#include <string>

namespace util {

class Log {
public:
    Log(const std::string &descriptor, bool enabled);
    bool isEnabled() const;
    void setEnabled(bool enabled);
    void disableLineBreaks();

    static bool loggingEnabled;
    static std::chrono::time_point<std::chrono::steady_clock> programStart;

protected:
    const std::string descriptor;
    bool lineBreaks;
    bool justPrintedLineBreak;
    bool enabled;
};

class ConsoleLog : public Log {
public:
    ConsoleLog(const std::string &descriptor, bool enabled);
    bool print(const std::string &message);

    std::ostream *os;
};

struct ConsoleLogPack {
    ConsoleLogPack();

    ConsoleLog bus;
    ConsoleLog cpu;
    ConsoleLog cdrom;
    ConsoleLog cp0RegisterRead;
    ConsoleLog cp0RegisterWrite;
    ConsoleLog dma;
    ConsoleLog dmaWrite;
    ConsoleLog dmaIO;
    ConsoleLog exception;
    ConsoleLog exceptionVerbose;
    ConsoleLog gpu;
    ConsoleLog gpuIO;
    ConsoleLog gpuVBLANK;
    ConsoleLog gpuVRAM;
    ConsoleLog gte;
    ConsoleLog gteVerbose;
    ConsoleLog instructions;
    ConsoleLog interrupts;
    ConsoleLog interruptsIO;
    ConsoleLog interruptsVerbose;
    ConsoleLog mdec;
    ConsoleLog memory;
    ConsoleLog misc;
    ConsoleLog peripheral;
    ConsoleLog registerRead;
    ConsoleLog registerWrite;
    ConsoleLog registerPCRead;
    ConsoleLog registerPCWrite;
    ConsoleLog renderer;
    ConsoleLog rendererVRAM;
    ConsoleLog spu;
    ConsoleLog timers;
    ConsoleLog warning;
};

extern ConsoleLogPack consoleLogPack;

#define MACRO_LOG(log) consoleLogPack.log.isEnabled() && consoleLogPack.log.print

#define LOG_BUS             MACRO_LOG(bus)
#define LOG_CPU             MACRO_LOG(cpu)
#define LOG_CDROM           MACRO_LOG(cdrom)
#define LOG_CP0_REG_READ    MACRO_LOG(cp0RegisterRead)
#define LOG_CP0_REG_WRITE   MACRO_LOG(cp0RegisterWrite)
#define LOG_DMA             MACRO_LOG(dma)
#define LOG_DMA_WRITE       MACRO_LOG(dmaWrite)
#define LOG_DMA_IO          MACRO_LOG(dmaIO)
#define LOG_EXC             MACRO_LOG(exception)
#define LOG_EXC_VERB        MACRO_LOG(exceptionVerbose)
#define LOG_GPU             MACRO_LOG(gpu)
#define LOG_GPU_IO          MACRO_LOG(gpuIO)
#define LOG_GPU_VBLANK      MACRO_LOG(gpuVBLANK)
#define LOG_GPU_VRAM        MACRO_LOG(gpuVRAM)
#define LOG_GTE             MACRO_LOG(gte)
#define LOG_GTE_VERB        MACRO_LOG(gteVerbose)
#define LOG_INS             MACRO_LOG(instructions)
#define LOG_INT             MACRO_LOG(interrupts)
#define LOG_INT_IO          MACRO_LOG(interruptsIO)
#define LOG_INT_VERB        MACRO_LOG(interruptsVerbose)
#define LOG_MDEC            MACRO_LOG(mdec)
#define LOG_MEM             MACRO_LOG(memory)
#define LOG_MISC            MACRO_LOG(misc)
#define LOG_PER             MACRO_LOG(peripheral)
#define LOG_REG_READ        MACRO_LOG(registerRead)
#define LOG_REG_WRITE       MACRO_LOG(registerWrite)
#define LOG_REG_PC_READ     MACRO_LOG(registerPCRead)
#define LOG_REG_PC_WRITE    MACRO_LOG(registerPCWrite)
#define LOG_REND            MACRO_LOG(renderer)
#define LOG_REND_VRAM       MACRO_LOG(rendererVRAM)
#define LOG_SPU             MACRO_LOG(spu)
#define LOG_TMR             MACRO_LOG(timers)
#define LOG_WRN             MACRO_LOG(warning)

}

#endif
