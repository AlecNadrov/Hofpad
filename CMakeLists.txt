cmake_minimum_required(VERSION 3.16)
project(Hofpad VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(hofpad
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/CodeEditor.cpp
    src/CodeEditor.h
    src/SettingsDialog.cpp
    src/SettingsDialog.h
    resources/resources.qrc
)

target_link_libraries(hofpad PRIVATE Qt6::Widgets)
