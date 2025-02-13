#include "log.h"

#include <iostream>

namespace util {

bool Log::loggingEnabled = true;

Log::Log(const std::string &descriptor, bool enabled)
    : descriptor(descriptor), lineBreaks(false), enabled(enabled) {
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
    : Log(descriptor, enabled) {
}

bool ConsoleLog::print(const std::string &message) {
    std::clog << "[" << descriptor << "] " << message
              << (lineBreaks ? "\n" : "") << std::flush;

    return false;
}

ConsoleLogPack consoleLogPack;

ConsoleLogPack::ConsoleLogPack()
    : bus("BUS", false),
      cpu("CPU", false),
      cdrom("CDROM", false),
      cp0RegisterRead("CPU", false),
      cp0RegisterWrite("CPU", false),
      dma("DMA", true),
      dmaWrite("DMA", false),
      dmaIO("DMA", false),
      exception("EXC", true),
      exceptionVerbose("EXC", false),
      gpu("GPU", true),
      gpuIO("GPU", false),
      gpuVRAM("GPU", true),
      interrupts("INT", true),
      interruptsIO("INT", false),
      mdec("MDEC", false),
      memory("MEM", false),
      misc("MISC", false),
      peripheral("PER", false),
      registerRead("CPU", false),
      registerWrite("CPU", false),
      registerPCRead("CPU", false),
      registerPCWrite("CPU", false),
      spu("SPU", false),
      timers("TMR", false),
      warning("WAR", true) {
    bus.disableLineBreaks();
    cpu.disableLineBreaks();
    cp0RegisterRead.disableLineBreaks();
    cp0RegisterWrite.disableLineBreaks();
    memory.disableLineBreaks();
    misc.disableLineBreaks();
    registerRead.disableLineBreaks();
    registerWrite.disableLineBreaks();
    registerPCRead.disableLineBreaks();
    registerPCWrite.disableLineBreaks();
}

bool Log::logEnabled = true;
bool Log::busLogEnabled = false;

void Log::log(const std::string &message, Type type) {
    switch (type) {
        case Type::WARNING:
            std::cerr << "Warning: " << message << std::endl;
            break;
        case Type::DMA:
        //case Type::DMA_WRITE:
            std::cerr << "[DMA] " << message << std::endl;
            break;
        case Type::GPU:
        //case Type::GPU_IO:
        case Type::GPU_VRAM:
            std::cerr << "[GPU] " <<  message << std::endl;;
            break;
        case Type::INTERRUPTS:
            std::cerr << "[INT] " <<  message << std::endl;;
            break;
        case Type::EXCEPTION:
            std::cerr <<  "[EXC] " << message << std::endl;
            break;
        case Type::CDROM:
        case Type::MDEC:
        case Type::PERIPHERAL:
        case Type::TIMERS:
        case Type::SPU:
        case Type::CPU:
        case Type::MEMORY:
        case Type::REGISTER_READ:
        case Type::REGISTER_WRITE:
        case Type::CP0_REGISTER_READ:
        case Type::CP0_REGISTER_WRITE:
        case Type::REGISTER_PC_WRITE:
        case Type::REGISTER_PC_READ:
        case Type::MISC:
            if (logEnabled) {
                std::cerr << message;
            }
            break;
        case Type::BUS:
            if (busLogEnabled) {
                std::cerr << message;
            }
        default:
            break;
    }
}

}
