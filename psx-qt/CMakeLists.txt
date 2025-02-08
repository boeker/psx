find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(psx-qt main.cpp)

target_sources(psx-qt PRIVATE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui)

target_link_libraries(psx-qt PRIVATE Qt6::Widgets)

