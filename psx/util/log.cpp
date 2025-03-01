#include "log.h"

#include <iostream>

namespace util {

bool Log::loggingEnabled = true;
std::chrono::time_point<std::chrono::steady_clock> Log::programStart = std::chrono::steady_clock::now();

Log::Log(const std::string &descriptor, bool enabled)
    : descriptor(descriptor), lineBreaks(true), justPrintedLineBreak(true), enabled(enabled) {
}

bool Log::isEnabled() const {
    return loggingEnabled && enabled;
}

void Log::setEnabled(bool enabled) {
    this->enabled = enabled;
}

void Log::disableLineBreaks() {
    this->lineBreaks = false;
}
ConsoleLog::ConsoleLog(const std::string &descriptor, bool enabled)
    : Log(descriptor, enabled),
      os(&std::clog) {
}

bool ConsoleLog::print(const std::string &message) {
    if (justPrintedLineBreak) {
        auto diff = std::chrono::steady_clock::now() - programStart;
        *os << std::format("{:%S} [{:s}] ", std::chrono::duration_cast<std::chrono::milliseconds>(diff), descriptor);
        justPrintedLineBreak = false;
    }

    *os << message;

    if (lineBreaks) {
        *os << "\n";
        justPrintedLineBreak = true;

    } else {
        if (message.find('\n') != std::string::npos) {
            justPrintedLineBreak = true;
        }
    }

    *os << std::flush;

    return false;
}

ConsoleLogPack consoleLogPack;

ConsoleLogPack::ConsoleLogPack()
    : bus("BUS", false),
      cpu("CPU", false),
      cdrom("CDROM", true),
      cp0RegisterRead("CPU", false),
      cp0RegisterWrite("CPU", false),
      dma("DMA", true),
      dmaWrite("DMA", true),
      dmaIO("DMA", false),
      exception("EXC", true),
      exceptionVerbose("EXC", false),
      gpu("GPU", true),
      gpuIO("GPU", true),
      gpuVBLANK("GPU", false),
      gpuVRAM("GPU", true),
      gte("GTE", true),
      gteVerbose("GTE", true),
      instructions("CPU", false),
      interrupts("INT", true),
      interruptsIO("INT", false),
      interruptsVerbose("INT", false),
      mdec("MDEC", false),
      memory("MEM", true),
      misc("MISC", false),
      peripheral("PER", false),
      registerRead("CPU", false),
      registerWrite("CPU", false),
      registerPCRead("CPU", false),
      registerPCWrite("CPU", false),
      renderer("REND", true),
      rendererVRAM("REND", true),
      spu("SPU", false),
      timers("TMR", false),
      warning("WRN", true) {
    bus.disableLineBreaks();
    cpu.disableLineBreaks();
    cp0RegisterRead.disableLineBreaks();
    cp0RegisterWrite.disableLineBreaks();
    instructions.disableLineBreaks();
    memory.disableLineBreaks();
    misc.disableLineBreaks();
    registerRead.disableLineBreaks();
    registerWrite.disableLineBreaks();
    registerPCRead.disableLineBreaks();
    registerPCWrite.disableLineBreaks();
}

}
