find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(psx-qt)

target_sources(psx-qt PRIVATE
    emuthread.cpp
    emuthread.h
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    openglwindow.cpp
    openglwindow.h
    vramviewerwindow.cpp
    vramviewerwindow.h
    vramviewerwindow.ui
)

target_include_directories(psx-qt PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/psx"
    "${CMAKE_SOURCE_DIR}/psx-qt"
)

target_link_libraries(psx-qt PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    psx
)

