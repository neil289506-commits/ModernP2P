cmake_minimum_required(VERSION 3.25)
project(ModernP2P VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network Test)
qt_standard_project_setup(REQUIRES 6.11)

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
