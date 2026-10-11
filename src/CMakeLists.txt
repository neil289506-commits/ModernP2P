add_library(p2p_core STATIC
    core/CodeGenerator.cpp
    core/CodeGenerator.h
)
target_include_directories(p2p_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(p2p_core PUBLIC Qt6::Core)

qt_add_executable(ModernP2P main.cpp)

qt_add_qml_module(ModernP2P
    URI ModernP2P
    VERSION 1.0
    QML_FILES ../qml/Main.qml
)

target_link_libraries(ModernP2P PRIVATE
    p2p_core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

set_target_properties(ModernP2P PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)
