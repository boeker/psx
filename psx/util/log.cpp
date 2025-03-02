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

OStreamLog::OStreamLog(std::ostream &os, const std::string &descriptor, bool enabled)
    : Log(enabled),
      os(os),
      descriptor(descriptor) {
}

bool OStreamLog::print(const std::string &message) {
    auto diff = std::chrono::steady_clock::now() - programStart;
    auto durationMS = std::chrono::duration_cast<std::chrono::milliseconds>(diff);
    os << std::format("{:%S} [{:s}] {:s}", durationMS, descriptor, message) << std::endl;

    return false;
}

ConsoleLog::ConsoleLog(const std::string &descriptor, bool enabled)
    : OStreamLog(std::clog, descriptor, enabled) {
}

FileLog::FileLog(const std::string &descriptor, bool enabled)
    : OStreamLog(logFile, descriptor, enabled) {
}

ThreeWayLog::ThreeWayLog(const std::string &descriptor, bool enabled)
    : fileLog(descriptor, enabled),
      consoleLog(descriptor, enabled) {
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
    if (fileLog.isEnabled()) {
        fileLog.print(message);
    }

    if (consoleLog.isEnabled()) {
        consoleLog.print(message);
    }

    if (additionalLog && additionalLog->isEnabled()) {
        additionalLog->print(message);
    }

    return false;
}

void ThreeWayLog::installAdditionalLog(std::shared_ptr<Log> log) {
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
      INIT_TWL(warning, "WRN") {
    misc.setConsoleLogEnabled(true);
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
    ENABLE_TWL_FILELOG(warning);
}

}
