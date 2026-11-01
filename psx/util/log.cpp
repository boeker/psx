#include "log.h"

#include <iostream>

namespace util {

std::ofstream log_file;

LogPack logPack;

LogPack::LogPack()
    : bus_log("BUS"),
      dsc_log("DSC"),
      img_log("IMG"),
      cp0_log("CP0"),
      cpu_log("CPU"),
      dma_log("DMA"),
      exc_log("EXC"),
      exe_log("EXE"),
      gio_log("GIO"),
      gpu_log("GPU"),
      gte_log("GTE"),
      int_log("INT"),
      mdc_log("MDC"),
      mem_log("MEM"),
      msc_log("MSC"),
      pad_log("PAD"),
      rnd_log("RND"),
      spu_log("SPU"),
      tmr_log("TMR"),
      tty_log("TTY") {
}

void LogPack::install_log_sink(const std::shared_ptr<LogSink> &log) {
    bus_log.set_ui_log(log);
    dsc_log.set_ui_log(log);
    img_log.set_ui_log(log);
    cp0_log.set_ui_log(log);
    cpu_log.set_ui_log(log);
    dma_log.set_ui_log(log);
    exc_log.set_ui_log(log);
    exe_log.set_ui_log(log);
    gio_log.set_ui_log(log);
    gpu_log.set_ui_log(log);
    gte_log.set_ui_log(log);
    int_log.set_ui_log(log);
    mdc_log.set_ui_log(log);
    mem_log.set_ui_log(log);
    msc_log.set_ui_log(log);
    pad_log.set_ui_log(log);
    rnd_log.set_ui_log(log);
    spu_log.set_ui_log(log);
    tmr_log.set_ui_log(log);
    tty_log.set_ui_log(log);
}

}
