#ifndef UTIL_LOG_H
#define UTIL_LOG_H

#include <chrono>
#include <fstream>
#include <memory>
#include <ostream>
#include <string>

namespace util {

class Log {
protected:
    static std::chrono::time_point<std::chrono::steady_clock> programStart;

public:
    Log(bool enabled);
    bool isEnabled() const;
    void setEnabled(bool enabled);

    virtual bool print(const std::string &message) = 0;

protected:
    bool enabled;
};

class OStreamLog : public Log {
public:
    OStreamLog(std::ostream &os, bool enabled);
    bool print(const std::string &message) override;

private:
    std::ostream &os;
};

class ConsoleLog : public OStreamLog {
public:
    ConsoleLog(bool enabled);
};

class FileLog : public OStreamLog {
public:
    FileLog(bool enabled);
    static std::ofstream logFile;
};

class ThreeWayLog {
public:
    ThreeWayLog(const std::string &descriptor, bool enabled);
    bool isEnabled() const;
    void setFileLogEnabled(bool enabled);
    void setConsoleLogEnabled(bool enabled);

    bool print(const std::string &message);

    void installAdditionalLog(const std::shared_ptr<Log> &log);

private:
    FileLog fileLog;
    ConsoleLog consoleLog;
    std::shared_ptr<Log> additionalLog;

    const std::string descriptor;
};

#define DECLARE_TWL(name) ThreeWayLog name, name##V, name##T

struct LogPack {
    LogPack();

    DECLARE_TWL(bus);
    DECLARE_TWL(cdrom);
    DECLARE_TWL(cpu);
    DECLARE_TWL(dma);
    DECLARE_TWL(exceptions);
    DECLARE_TWL(executable);
    DECLARE_TWL(gpu);
    DECLARE_TWL(gte);
    DECLARE_TWL(interrupts);
    DECLARE_TWL(mdec);
    DECLARE_TWL(memory);
    DECLARE_TWL(misc);
    DECLARE_TWL(peripheral);
    DECLARE_TWL(renderer);
    DECLARE_TWL(spu);
    DECLARE_TWL(timers);
    DECLARE_TWL(tty);
    DECLARE_TWL(warning);

    void enableAllFileLogging();
    void installAdditionalLog(const std::shared_ptr<Log> &log);
};

extern LogPack logPack;

#define MACRO_LOG(log) util::logPack.log.isEnabled() && util::logPack.log.print

#define LOG_BUS             MACRO_LOG(bus)
#define LOGV_BUS            MACRO_LOG(busV)
#define LOGT_BUS            MACRO_LOG(busT)

#define LOG_CPU             MACRO_LOG(cpu)
#define LOGV_CPU            MACRO_LOG(cpuV)
#define LOGT_CPU            MACRO_LOG(cpuT)

#define LOG_CDROM           MACRO_LOG(cdrom)
#define LOGV_CDROM          MACRO_LOG(cdromV)
#define LOGT_CDROM          MACRO_LOG(cdromT)

#define LOG_DMA             MACRO_LOG(dma)
#define LOGV_DMA            MACRO_LOG(dmaV)
#define LOGT_DMA            MACRO_LOG(dmaT)

#define LOG_EXC             MACRO_LOG(exceptions)
#define LOGV_EXC            MACRO_LOG(exceptionsV)
#define LOGT_EXC            MACRO_LOG(exceptionsT)

#define LOG_EXE             MACRO_LOG(executable)
#define LOGV_EXE            MACRO_LOG(executableV)
#define LOGT_EXE            MACRO_LOG(executableT)

#define LOG_GPU             MACRO_LOG(gpu)
#define LOGV_GPU            MACRO_LOG(gpuV)
#define LOGT_GPU            MACRO_LOG(gpuT)

#define LOG_GTE             MACRO_LOG(gte)
#define LOGV_GTE            MACRO_LOG(gteV)
#define LOGT_GTE            MACRO_LOG(gteT)

#define LOG_INT             MACRO_LOG(interrupts)
#define LOGV_INT            MACRO_LOG(interruptsV)
#define LOGT_INT            MACRO_LOG(interruptsT)

#define LOG_MDEC            MACRO_LOG(mdec)
#define LOGV_MDEC           MACRO_LOG(mdecV)
#define LOGT_MDEC           MACRO_LOG(mdecT)

#define LOG_MEM             MACRO_LOG(memory)
#define LOGV_MEM            MACRO_LOG(memoryV)
#define LOGT_MEM            MACRO_LOG(memoryT)

#define LOG_MISC            MACRO_LOG(misc)
#define LOGV_MISC           MACRO_LOG(miscV)
#define LOGT_MISC           MACRO_LOG(miscT)

#define LOG_PER             MACRO_LOG(peripheral)
#define LOGV_PER            MACRO_LOG(peripheralV)
#define LOGT_PER            MACRO_LOG(peripheralT)

#define LOG_REND            MACRO_LOG(renderer)
#define LOGV_REND           MACRO_LOG(rendererV)
#define LOGT_REND           MACRO_LOG(rendererT)

#define LOG_SPU             MACRO_LOG(spu)
#define LOGV_SPU            MACRO_LOG(spuV)
#define LOGT_SPU            MACRO_LOG(spuT)

#define LOG_TMR             MACRO_LOG(timers)
#define LOGV_TMR            MACRO_LOG(timersV)
#define LOGT_TMR            MACRO_LOG(timersT)

#define LOG_TTY             MACRO_LOG(tty)
#define LOGV_TTY            MACRO_LOG(ttyV)
#define LOGT_TTY            MACRO_LOG(ttyT)

#define LOG_WRN             MACRO_LOG(warning)
#define LOGV_WRN            MACRO_LOG(warningV)
#define LOGT_WRN            MACRO_LOG(warningT)


}

#endif
