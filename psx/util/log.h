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
