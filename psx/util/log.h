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

struct LogPack {
    LogPack();

    void install_log_sink(const std::shared_ptr<LogSink>& log);

    constexpr static LogLevel max_level_ui = LogLevel::WARNING;
    constexpr static LogLevel max_level_console = LogLevel::INFO;
    constexpr static LogLevel max_level_file = LogLevel::NONE;

    NewLog<max_level_ui,   max_level_console, max_level_file> bus_log; // Bus
    NewLog<max_level_ui,   max_level_console, max_level_file> dsc_log; // Disc
    NewLog<max_level_ui,   max_level_console, max_level_file> img_log; // Disc Image
    NewLog<max_level_ui,   max_level_console, max_level_file> cp0_log; // CP0
    NewLog<max_level_ui,   max_level_console, max_level_file> cpu_log; // CPU
    NewLog<max_level_ui,   max_level_console, max_level_file> dma_log; // DMA
    NewLog<max_level_ui,   max_level_console, max_level_file> exc_log; // Exceptions
    NewLog<max_level_ui,   max_level_console, max_level_file> exe_log; // Executable
    NewLog<max_level_ui,   max_level_console, max_level_file> gio_log; // Gamepad I/O
    NewLog<max_level_ui,   max_level_console, max_level_file> gpu_log; // GPU
    NewLog<max_level_ui,   max_level_console, max_level_file> gte_log; // GTE
    NewLog<max_level_ui,   max_level_console, max_level_file> int_log; // Interrupts
    NewLog<max_level_ui,   max_level_console, max_level_file> mdc_log; // MDEC
    NewLog<max_level_ui,   max_level_console, max_level_file> mem_log; // Memory
    NewLog<LogLevel::INFO, max_level_console, max_level_file> msc_log; // Misc
    NewLog<max_level_ui,   max_level_console, max_level_file> pad_log; // Gamepad
    NewLog<max_level_ui,   max_level_console, max_level_file> rnd_log; // Renderer
    NewLog<max_level_ui,   max_level_console, max_level_file> spu_log; // SPU
    NewLog<max_level_ui,   max_level_console, max_level_file> tmr_log; // Timers
    NewLog<LogLevel::INFO, max_level_console, max_level_file> tty_log; // TTY
};

extern LogPack logPack;

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
