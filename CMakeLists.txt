cmake_minimum_required(VERSION 3.25)
project(ModernP2P VERSION 0.1.0 LANGUAGES CXX)
if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Base URL of the deployed Cloudflare Worker, e.g. https://modernp2p-api.<subdomain>.workers.dev
set(P2P_API_BASE "" CACHE STRING "Cloudflare Worker base URL baked into the app")
# Minimum Qt version (CI uses 6.11.0; lower it only for local experiments).
set(P2P_QT_MIN 6.11 CACHE STRING "Minimum Qt version")
option(P2P_BUILD_APP "Build the GUI application (needs WebEngine)" ON)
option(P2P_BUILD_SCREENSHOTS "Build the tool that renders README screenshots with demo data" OFF)

set(_qt_components Core Network WebSockets Test)
if(P2P_BUILD_APP)
    list(APPEND _qt_components Gui Qml Quick QuickControls2 WebChannel WebEngineCore)
endif()
find_package(Qt6 ${P2P_QT_MIN} REQUIRED COMPONENTS ${_qt_components})
qt_standard_project_setup()

if(MSVC)
    add_compile_options(/utf-8)
endif()

enable_testing()
add_subdirectory(src)
add_subdirectory(tests)
if(P2P_BUILD_SCREENSHOTS)
    add_subdirectory(tools/screenshots)
endif()
