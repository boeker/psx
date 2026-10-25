#ifndef UTIL_LOG_H
#define UTIL_LOG_H

#include <chrono>
#include <format>
#include <fstream>
#include <memory>
#include <iostream>
#include <string>
#include <utility>

namespace util {

enum LogLevel {
    NONE,
    WARNING,
    INFO,
    DEBUG,
    TRACE,
    TRACE_DETAIL
};

extern std::ofstream log_file;

class LogSink {
public:
    virtual bool print(const std::string &message) = 0;
};


template<uint32_t UI_LEVEL, uint32_t CONSOLE_LEVEL, uint32_t FILE_LEVEL>
class NewLog {
private:
    const std::string prefix;

    std::shared_ptr<LogSink> ui_log;
    std::ostream* file_stream;

public:
    NewLog(const std::string& descriptor)
        : prefix(std::format("[{:s}] ", descriptor)),
          ui_log(),
          file_stream(nullptr) {
    }

    void set_ui_log(std::shared_ptr<LogSink> ui_log) {
        this->ui_log = std::move(ui_log);
    }

    template <uint32_t level, typename... Args>
    inline void log(std::format_string<Args...> message, Args&&... args) const {
        if constexpr (level <= UI_LEVEL || level <= CONSOLE_LEVEL || level <= FILE_LEVEL) {
            std::string formatted = std::format("{:s}{:s}", prefix, std::format(message, std::forward<Args>(args)...));

            if constexpr (level <= UI_LEVEL) {
                if (ui_log) {
                    ui_log->print(formatted);
                }
            }

            if constexpr (level <= CONSOLE_LEVEL) {
                std::clog << formatted << std::endl;
            }

            if constexpr (level <= FILE_LEVEL) {
                if (file_stream) {
                    (*file_stream) << formatted << std::endl;
                }
            }
        }
    }
};

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

#define DECLARE_TWL(name) ThreeWayLog name, name##W, name##V, name##T

struct LogPack {
    LogPack();

    DECLARE_TWL(bus);
    DECLARE_TWL(cdimg);
    DECLARE_TWL(cdrom);
    DECLARE_TWL(cp0);
    DECLARE_TWL(cpu);
    DECLARE_TWL(dma);
    DECLARE_TWL(exceptions);
    DECLARE_TWL(executable);
    DECLARE_TWL(gio);
    DECLARE_TWL(gpu);
    DECLARE_TWL(gte);
    DECLARE_TWL(interrupts);
    DECLARE_TWL(mdec);
    DECLARE_TWL(memory);
    DECLARE_TWL(misc);
    DECLARE_TWL(pad);
    DECLARE_TWL(renderer);
    DECLARE_TWL(spu);
    DECLARE_TWL(timers);
    DECLARE_TWL(tty);
    DECLARE_TWL(warning);

    void enableAllFileLogging();
    void installAdditionalLog(const std::shared_ptr<Log> &log);

    void install_log_sink(const std::shared_ptr<LogSink>& log);

    constexpr static LogLevel file_level = LogLevel::NONE;
    NewLog<LogLevel::INFO, LogLevel::INFO, file_level> bus_log; // Bus
    NewLog<LogLevel::INFO, LogLevel::INFO, file_level> dsc_log; // Disc
    NewLog<LogLevel::INFO, LogLevel::INFO, file_level> img_log; // Disc Image
    NewLog<LogLevel::INFO, LogLevel::INFO, file_level> cp0_log; // CP0
    NewLog<LogLevel::INFO, LogLevel::INFO, file_level> cpu_log; // CPU
    NewLog<LogLevel::INFO, LogLevel::INFO, file_level> dma_log; // DMA
    NewLog<LogLevel::INFO, LogLevel::INFO, file_level> exc_log; // Exceptions
    NewLog<LogLevel::INFO, LogLevel::INFO, file_level> exe_log; // Executable
    NewLog<LogLevel::INFO, LogLevel::INFO, file_level> gio_log; // Gamepad I/O
    NewLog<LogLevel::INFO, LogLevel::INFO, file_level> gpu_log; // GPU
    NewLog<LogLevel::INFO, LogLevel::INFO, file_level> gte_log; // GTE
    NewLog<LogLevel::INFO, LogLevel::INFO, file_level> int_log; // Interrupts
    NewLog<LogLevel::INFO, LogLevel::INFO, file_level> mdc_log; // MDEC
    NewLog<LogLevel::INFO, LogLevel::INFO, file_level> mem_log; // Memory
    NewLog<LogLevel::INFO, LogLevel::INFO, file_level> msc_log; // Misc
    NewLog<LogLevel::INFO, LogLevel::INFO, file_level> pad_log; // Gamepad
    NewLog<LogLevel::INFO, LogLevel::INFO, file_level> rnd_log; // Renderer
    NewLog<LogLevel::INFO, LogLevel::INFO, file_level> spu_log; // SPU
    NewLog<LogLevel::INFO, LogLevel::INFO, file_level> tmr_log; // Timers
    NewLog<LogLevel::INFO, LogLevel::INFO, file_level> tty_log; // TTY
};

extern LogPack logPack;

#define MACRO_LOG(log) util::logPack.log.isEnabled() && util::logPack.log.print

#define LOG_BUS             MACRO_LOG(bus)
#define LOGW_BUS            MACRO_LOG(busW)
#define LOGV_BUS            MACRO_LOG(busV)
#define LOGT_BUS            MACRO_LOG(busT)

#define LOG_CP0             MACRO_LOG(cp0)
#define LOGW_CP0            MACRO_LOG(cp0W)
#define LOGV_CP0            MACRO_LOG(cp0V)
#define LOGT_CP0            MACRO_LOG(cp0T)

#define LOG_CPU             MACRO_LOG(cpu)
#define LOGW_CPU            MACRO_LOG(cpuW)
#define LOGV_CPU            MACRO_LOG(cpuV)
#define LOGT_CPU            MACRO_LOG(cpuT)

#define LOG_CDIMG           MACRO_LOG(cdimg)
#define LOGW_CDIMG          MACRO_LOG(cdimgW)
#define LOGV_CDIMG          MACRO_LOG(cdimgV)
#define LOGT_CDIMG          MACRO_LOG(cdimgT)

#define LOG_CDROM           MACRO_LOG(cdrom)
#define LOGW_CDROM          MACRO_LOG(cdromW)
#define LOGV_CDROM          MACRO_LOG(cdromV)
#define LOGT_CDROM          MACRO_LOG(cdromT)

#define LOG_DMA             MACRO_LOG(dma)
#define LOGW_DMA            MACRO_LOG(dmaW)
#define LOGV_DMA            MACRO_LOG(dmaV)
#define LOGT_DMA            MACRO_LOG(dmaT)

#define LOG_EXC             MACRO_LOG(exceptions)
#define LOGW_EXC            MACRO_LOG(exceptionsW)
#define LOGV_EXC            MACRO_LOG(exceptionsV)
#define LOGT_EXC            MACRO_LOG(exceptionsT)

#define LOG_EXE             MACRO_LOG(executable)
#define LOGW_EXE            MACRO_LOG(executableW)
#define LOGV_EXE            MACRO_LOG(executableV)
#define LOGT_EXE            MACRO_LOG(executableT)

#define LOG_GIO             MACRO_LOG(gio)
#define LOGW_GIO            MACRO_LOG(gioW)
#define LOGV_GIO            MACRO_LOG(gioV)
#define LOGT_GIO            MACRO_LOG(gioT)

#define LOG_GPU             MACRO_LOG(gpu)
#define LOGW_GPU            MACRO_LOG(gpuW)
#define LOGV_GPU            MACRO_LOG(gpuV)
#define LOGT_GPU            MACRO_LOG(gpuT)

#define LOG_GTE             MACRO_LOG(gte)
#define LOGW_GTE            MACRO_LOG(gteW)
#define LOGV_GTE            MACRO_LOG(gteV)
#define LOGT_GTE            MACRO_LOG(gteT)

#define LOG_INT             MACRO_LOG(interrupts)
#define LOGW_INT            MACRO_LOG(interruptsW)
#define LOGV_INT            MACRO_LOG(interruptsV)
#define LOGT_INT            MACRO_LOG(interruptsT)

#define LOG_MDEC            MACRO_LOG(mdec)
#define LOGW_MDEC           MACRO_LOG(mdecW)
#define LOGV_MDEC           MACRO_LOG(mdecV)
#define LOGT_MDEC           MACRO_LOG(mdecT)

#define LOG_MEM             MACRO_LOG(memory)
#define LOGW_MEM            MACRO_LOG(memoryW)
#define LOGV_MEM            MACRO_LOG(memoryV)
#define LOGT_MEM            MACRO_LOG(memoryT)

#define LOG_MISC            MACRO_LOG(misc)
#define LOGW_MISC           MACRO_LOG(miscW)
#define LOGV_MISC           MACRO_LOG(miscV)
#define LOGT_MISC           MACRO_LOG(miscT)

#define LOG_PAD             MACRO_LOG(pad)
#define LOGW_PAD            MACRO_LOG(padW)
#define LOGV_PAD            MACRO_LOG(padV)
#define LOGT_PAD            MACRO_LOG(padT)

#define LOG_REND            MACRO_LOG(renderer)
#define LOGW_REND           MACRO_LOG(rendererW)
#define LOGV_REND           MACRO_LOG(rendererV)
#define LOGT_REND           MACRO_LOG(rendererT)

#define LOG_SPU             MACRO_LOG(spu)
#define LOGW_SPU            MACRO_LOG(spuW)
#define LOGV_SPU            MACRO_LOG(spuV)
#define LOGT_SPU            MACRO_LOG(spuT)

#define LOG_TMR             MACRO_LOG(timers)
#define LOGW_TMR            MACRO_LOG(timersW)
#define LOGV_TMR            MACRO_LOG(timersV)
#define LOGT_TMR            MACRO_LOG(timersT)

#define LOG_TTY             MACRO_LOG(tty)
#define LOGW_TTY            MACRO_LOG(ttyW)
#define LOGV_TTY            MACRO_LOG(ttyV)
#define LOGT_TTY            MACRO_LOG(ttyT)

#define LOG_WRN             MACRO_LOG(warning)
#define LOGW_WRN            MACRO_LOG(warningW)
#define LOGV_WRN            MACRO_LOG(warningV)
#define LOGT_WRN            MACRO_LOG(warningT)

#define LOGL_BUS(level, ...) util::logPack.bus_log.log<level>(__VA_ARGS__)
#define LOGL_DSC(level, ...) util::logPack.dsc_log.log<level>(__VA_ARGS__)
#define LOGL_IMG(level, ...) util::logPack.img_log.log<level>(__VA_ARGS__)
#define LOGL_CP0(level, ...) util::logPack.cp0_log.log<level>(__VA_ARGS__)
#define LOGL_CPU(level, ...) util::logPack.cpu_log.log<level>(__VA_ARGS__)
#define LOGL_DMA(level, ...) util::logPack.dma_log.log<level>(__VA_ARGS__)
#define LOGL_EXC(level, ...) util::logPack.exc_log.log<level>(__VA_ARGS__)
#define LOGL_EXE(level, ...) util::logPack.exe_log.log<level>(__VA_ARGS__)
#define LOGL_GIO(level, ...) util::logPack.gio_log.log<level>(__VA_ARGS__)
#define LOGL_GPU(level, ...) util::logPack.gpu_log.log<level>(__VA_ARGS__)
#define LOGL_GTE(level, ...) util::logPack.gte_log.log<level>(__VA_ARGS__)
#define LOGL_INT(level, ...) util::logPack.int_log.log<level>(__VA_ARGS__)
#define LOGL_MDC(level, ...) util::logPack.mdc_log.log<level>(__VA_ARGS__)
#define LOGL_MEM(level, ...) util::logPack.mem_log.log<level>(__VA_ARGS__)
#define LOGL_MSC(level, ...) util::logPack.msc_log.log<level>(__VA_ARGS__)
#define LOGL_PAD(level, ...) util::logPack.pad_log.log<level>(__VA_ARGS__)
#define LOGL_RND(level, ...) util::logPack.rnd_log.log<level>(__VA_ARGS__)
#define LOGL_SPU(level, ...) util::logPack.spu_log.log<level>(__VA_ARGS__)
#define LOGL_TMR(level, ...) util::logPack.tmr_log.log<level>(__VA_ARGS__)
#define LOGL_TTY(level, ...) util::logPack.tty_log.log<level>(__VA_ARGS__)

}

#endif
