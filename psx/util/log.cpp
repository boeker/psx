#include "log.h"

#include <iostream>

namespace util {

std::chrono::time_point<std::chrono::steady_clock> Log::programStart = std::chrono::steady_clock::now();

std::ofstream FileLog::logFile;

Log::Log(bool enabled)
    : enabled(enabled) {
}

bool Log::isEnabled() const {
    return enabled;
}

void Log::setEnabled(bool enabled) {
    this->enabled = enabled;
}

OStreamLog::OStreamLog(std::ostream &os, bool enabled)
    : Log(enabled),
      os(os) {
}

bool OStreamLog::print(const std::string &message) {
    os << message << std::endl;

    return false;
}

ConsoleLog::ConsoleLog(bool enabled)
    : OStreamLog(std::clog, enabled) {
}

FileLog::FileLog(bool enabled)
    : OStreamLog(logFile, enabled) {
}

ThreeWayLog::ThreeWayLog(const std::string &descriptor, bool enabled)
    : fileLog(enabled),
      consoleLog(enabled),
      descriptor(descriptor) {
}

bool ThreeWayLog::isEnabled() const {
    return fileLog.isEnabled()
           || consoleLog.isEnabled()
           || (additionalLog != nullptr && additionalLog->isEnabled());
}

void ThreeWayLog::setFileLogEnabled(bool enabled) {
    fileLog.setEnabled(enabled);
}

void ThreeWayLog::setConsoleLogEnabled(bool enabled) {
    consoleLog.setEnabled(enabled);
}

bool ThreeWayLog::print(const std::string &message) {
    std::string messageWithDescriptor = std::format("[{:s}] {:s}", descriptor, message);

    if (fileLog.isEnabled()) {
        fileLog.print(messageWithDescriptor);
    }

    if (consoleLog.isEnabled()) {
        consoleLog.print(messageWithDescriptor);
    }

    if (additionalLog && additionalLog->isEnabled()) {
        additionalLog->print(messageWithDescriptor);
    }

    return false;
}

void ThreeWayLog::installAdditionalLog(const std::shared_ptr<Log> &log) {
    additionalLog = log;
}

#define INIT_TWL(name, descriptor) name(descriptor, false), name##V(descriptor, false), name##T(descriptor, false)

LogPack logPack;

LogPack::LogPack()
    : INIT_TWL(bus, "BUS"),
      INIT_TWL(cdrom, "CDROM"),
      INIT_TWL(cpu, "CPU"),
      INIT_TWL(dma, "DMA"),
      INIT_TWL(exceptions, "EXC"),
      INIT_TWL(executable, "EXE"),
      INIT_TWL(gpu, "GPU"),
      INIT_TWL(gte, "GTE"),
      INIT_TWL(interrupts, "INT"),
      INIT_TWL(mdec, "MDEC"),
      INIT_TWL(memory, "MEM"),
      INIT_TWL(misc, "MISC"),
      INIT_TWL(peripheral, "PER"),
      INIT_TWL(renderer, "REND"),
      INIT_TWL(spu, "SPU"),
      INIT_TWL(timers, "TMR"),
      INIT_TWL(tty, "TTY"),
      INIT_TWL(warning, "WRN") {
    executable.setConsoleLogEnabled(true);
    misc.setConsoleLogEnabled(true);
    tty.setConsoleLogEnabled(true);
    timers.setConsoleLogEnabled(true);
    //timersV.setConsoleLogEnabled(true);
    //timersT.setConsoleLogEnabled(true);
    //exceptions.setConsoleLogEnabled(true);
    //exceptionsV.setConsoleLogEnabled(true);
    //FileLog::logFile.open("trace.txt");
    //enableAllFileLogging();
}

#define ENABLE_TWL_FILELOG(name) name.setFileLogEnabled(true); name##V.setFileLogEnabled(true); name##T.setFileLogEnabled(true)

void LogPack::enableAllFileLogging() {
    ENABLE_TWL_FILELOG(bus);
    ENABLE_TWL_FILELOG(cdrom);
    ENABLE_TWL_FILELOG(cpu);
    ENABLE_TWL_FILELOG(dma);
    ENABLE_TWL_FILELOG(exceptions);
    ENABLE_TWL_FILELOG(executable);
    ENABLE_TWL_FILELOG(gpu);
    ENABLE_TWL_FILELOG(gte);
    ENABLE_TWL_FILELOG(interrupts);
    ENABLE_TWL_FILELOG(mdec);
    ENABLE_TWL_FILELOG(memory);
    ENABLE_TWL_FILELOG(misc);
    ENABLE_TWL_FILELOG(peripheral);
    ENABLE_TWL_FILELOG(renderer);
    ENABLE_TWL_FILELOG(spu);
    ENABLE_TWL_FILELOG(timers);
    ENABLE_TWL_FILELOG(tty);
    ENABLE_TWL_FILELOG(warning);
}

void LogPack::installAdditionalLog(const std::shared_ptr<Log> &log) {
    bus.installAdditionalLog(log);
    cdrom.installAdditionalLog(log);
    cpu.installAdditionalLog(log);
    dma.installAdditionalLog(log);
    exceptions.installAdditionalLog(log);
    executable.installAdditionalLog(log);
    gpu.installAdditionalLog(log);
    gte.installAdditionalLog(log);
    interrupts.installAdditionalLog(log);
    mdec.installAdditionalLog(log);
    memory.installAdditionalLog(log);
    misc.installAdditionalLog(log);
    peripheral.installAdditionalLog(log);
    renderer.installAdditionalLog(log);
    spu.installAdditionalLog(log);
    timers.installAdditionalLog(log);
    tty.installAdditionalLog(log);
    warning.installAdditionalLog(log);

    //timersV.installAdditionalLog(log);
    //timersT.installAdditionalLog(log);

    //exceptionsV.installAdditionalLog(log);

    //gpuV.installAdditionalLog(log);
    //gpuT.installAdditionalLog(log);
}

}
