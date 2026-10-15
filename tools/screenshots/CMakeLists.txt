# Renders src/qml/Main.qml with demo data (no network, no WebEngine) to PNG files.
find_package(Qt6 ${P2P_QT_MIN} REQUIRED COMPONENTS Gui Qml Quick QuickControls2)

qt_add_executable(p2p_screenshots main.cpp)
target_link_libraries(p2p_screenshots PRIVATE p2p_core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
