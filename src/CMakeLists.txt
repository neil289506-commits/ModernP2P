# Qt-only core (no GUI/WebEngine) so it can be unit tested anywhere.
add_library(p2p_core STATIC
    core/CodeGenerator.cpp core/CodeGenerator.h
    core/FileNames.cpp core/FileNames.h
    core/Identity.cpp core/Identity.h
    core/Logger.cpp core/Logger.h
    core/Version.cpp core/Version.h
    models/StagingModel.cpp models/StagingModel.h
    models/RequestsModel.cpp models/RequestsModel.h
    models/TransferModel.cpp models/TransferModel.h
    models/HistoryModel.cpp models/HistoryModel.h
    models/BlockListModel.cpp models/BlockListModel.h
    transfer/PeerTransport.h
    transfer/TransferEngine.cpp transfer/TransferEngine.h
)
target_include_directories(p2p_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(p2p_core PUBLIC Qt6::Core)
set_target_properties(p2p_core PROPERTIES AUTOMOC ON)

if(P2P_BUILD_APP)
    qt_add_executable(ModernP2P
        main.cpp
        network/CloudflareClient.cpp network/CloudflareClient.h
        network/UpdateChecker.cpp network/UpdateChecker.h
        transfer/PeerBridge.cpp transfer/PeerBridge.h
        app/AppController.cpp app/AppController.h
        app/SelfTest.cpp app/SelfTest.h
    )

    qt_add_qml_module(ModernP2P
        URI ModernP2P
        VERSION 1.0
        QML_FILES qml/Main.qml
    )

    # PeerJS host page and the vendored PeerJS build, served from qrc:/web/.
    qt_add_resources(ModernP2P web
        PREFIX "/web"
        BASE ${CMAKE_SOURCE_DIR}/web
        FILES
            ${CMAKE_SOURCE_DIR}/web/bridge.html
            ${CMAKE_SOURCE_DIR}/web/bridge.js
    )
    qt_add_resources(ModernP2P peerjs
        PREFIX "/web"
        BASE ${CMAKE_SOURCE_DIR}/third_party/peerjs
        FILES ${CMAKE_SOURCE_DIR}/third_party/peerjs/peerjs.min.js
    )

    qt_add_resources(ModernP2P icons
        PREFIX "/icons"
        BASE ${CMAKE_SOURCE_DIR}/resources
        FILES ${CMAKE_SOURCE_DIR}/resources/icon.png
    )
    if(WIN32)
        target_sources(ModernP2P PRIVATE app.rc)   # embeds the .exe icon
    endif()

    qt_add_translations(ModernP2P TS_FILES ../i18n/ModernP2P_zh_TW.ts)

    target_compile_definitions(ModernP2P PRIVATE P2P_API_BASE="${P2P_API_BASE}" P2P_VERSION="${PROJECT_VERSION}")
    target_link_libraries(ModernP2P PRIVATE
        p2p_core
        Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
        Qt6::Network Qt6::WebSockets Qt6::WebChannel Qt6::WebEngineCore
    )
    set_target_properties(ModernP2P PROPERTIES WIN32_EXECUTABLE ON)
endif()
