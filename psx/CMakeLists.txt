add_library(psx)

target_compile_features(psx PUBLIC cxx_std_20)

target_sources(psx PRIVATE
    bios.cpp
    bus.cpp
    cdrom.cpp
    core.cpp
    cp0registers.cpp
    cpu.cpp
    dma.cpp
    executable.cpp
    gio.cpp
    gamepad.cpp
    gpu.cpp
    gte.cpp
    interrupts.cpp
    memory.cpp
    registers.cpp
    renderer/null/nullrenderer.cpp
    renderer/opengl/gl.cpp
    renderer/opengl/glad.cpp
    renderer/opengl/openglrenderer.cpp
    renderer/opengl/shader.cpp
    renderer/software/gl.cpp
    renderer/software/glad.cpp
    renderer/software/softwarerenderer.cpp
    renderer/software/shader.cpp
    spu.cpp
    timers.cpp
    util/disassembler.cpp
    util/log.cpp
)

target_include_directories(psx PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/psx"
)

