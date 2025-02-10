add_library(psx)

target_compile_features(psx PUBLIC cxx_std_20)

target_sources(psx PRIVATE
    bios.cpp
    bios.h
    bus.cpp
    bus.h
    core.cpp
    core.h
    cp0registers.cpp
    cp0registers.h
    cpu.cpp
    cpu.h
    dma.cpp
    dma.h
    exceptions/exceptions.h
    gl/glad.cpp
    gl/glrender.cpp
    gl/glrender.h
    gpu.cpp
    gpu.h
    interrupts.cpp
    interrupts.h
    memory.cpp
    memory.h
    registers.cpp
    registers.h
    screen.h
    spu.cpp
    spu.h
    timers.cpp
    timers.h
    util/log.cpp
    util/log.h
)

target_include_directories(psx PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/psx"
)

