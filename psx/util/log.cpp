#include "log.h"

#include <iostream>

namespace util {

bool Log::loggingEnabled = true;
std::chrono::time_point<std::chrono::steady_clock> Log::programStart = std::chrono::steady_clock::now();

std::ofstream FileLog::logFile("trace.txt");

Log::Log(bool enabled)
    : enabled(enabled) {
}

bool Log::isEnabled() const {
    return loggingEnabled && enabled;
}

void Log::setEnabled(bool enabled) {
    this->enabled = enabled;
}

OStreamLog::OStreamLog(std::ostream &os, const std::string &descriptor, bool enabled)
    : Log(enabled),
      os(os) {
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

bool ThreeWayLog::print(const std::string &message) {
    fileLog.print(message);

    consoleLog.print(message);

    if (additionalLog) {
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
}

}
