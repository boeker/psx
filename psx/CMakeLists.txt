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
    gpu.cpp
    gpu.h
    interrupts.cpp
    interrupts.h
    memory.cpp
    memory.h
    registers.cpp
    registers.h
    renderer/null/nullrenderer.cpp
    renderer/null/nullrenderer.h
    renderer/opengl/glad.cpp
    renderer/opengl/openglrenderer.cpp
    renderer/opengl/openglrenderer.h
    renderer/renderer.h
    renderer/screen.h
    spu.cpp
    spu.h
    timers.cpp
    timers.h
    util/bit.h
    util/log.cpp
    util/log.h
)

target_include_directories(psx PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/psx"
)

