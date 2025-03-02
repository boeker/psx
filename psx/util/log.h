#ifndef UTIL_LOG_H
#define UTIL_LOG_H

#include <chrono>
#include <fstream>
#include <memory>
#include <ostream>
#include <string>

namespace util {

class Log {
public:
    static bool loggingEnabled;

protected:
    static std::chrono::time_point<std::chrono::steady_clock> programStart;

public:
    Log(bool enabled);
    bool isEnabled() const;
    void setEnabled(bool enabled);

    virtual bool print(const std::string &message, int verbosityLevel = 0) = 0;

protected:
    bool enabled;
};

class OStreamLog : public Log {
public:
    OStreamLog(std::ostream &os, const std::string &descriptor, bool enabled);
    bool print(const std::string &message, int verbosityLevel) override;

private:
    std::ostream &os;

    const std::string descriptor;
};

class ConsoleLog : public OStreamLog {
public:
    ConsoleLog(const std::string &descriptor, bool enabled);
    bool print(const std::string &message, int verbosityLevel) override;

    void setVerbosityLevel(int verbosityLevel);

private:
    int verbosityLevel;
};

//    void disableAutoLineBreaks();
//    void produceLineBreak();

class FileTrace : public OStreamLog {
private:
    static std::ofstream logFile;

public:
    FileTrace(const std::string &descriptor, bool enabled);
};

class ThreeWayLog : public Log {
public:
    ThreeWayLog(const std::string &descriptor, bool enabled);
    bool print(const std::string &message, int verbosityLevel = 0) override;

    void installAdditionalLog(std::shared_ptr<Log> log);

private:
    ConsoleLog consoleLog;
    FileTrace fileTrace;

    std::shared_ptr<Log> additionalLog;
};


struct LogPack {
    LogPack();

    ThreeWayLog bus;
    ThreeWayLog cpu;
    ThreeWayLog cdrom;
    ThreeWayLog cp0RegisterRead;
    ThreeWayLog cp0RegisterWrite;
    ThreeWayLog dma;
    ThreeWayLog dmaWrite;
    ThreeWayLog dmaIO;
    ThreeWayLog exception;
    ThreeWayLog exceptionVerbose;
    ThreeWayLog gpu;
    ThreeWayLog gpuIO;
    ThreeWayLog gpuVBLANK;
    ThreeWayLog gpuVRAM;
    ThreeWayLog gte;
    ThreeWayLog gteVerbose;
    ThreeWayLog instructions;
    ThreeWayLog interrupts;
    ThreeWayLog interruptsIO;
    ThreeWayLog interruptsVerbose;
    ThreeWayLog mdec;
    ThreeWayLog memory;
    ThreeWayLog misc;
    ThreeWayLog peripheral;
    ThreeWayLog registerRead;
    ThreeWayLog registerWrite;
    ThreeWayLog registerPCRead;
    ThreeWayLog registerPCWrite;
    ThreeWayLog renderer;
    ThreeWayLog rendererVRAM;
    ThreeWayLog spu;
    ThreeWayLog timers;
    ThreeWayLog warning;
};

extern LogPack logPack;

#define MACRO_LOG(log) logPack.log.isEnabled() && logPack.log.print

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
