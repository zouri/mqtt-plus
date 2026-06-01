cmake_minimum_required(VERSION 3.24)

project(mqtt-plus VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2 Sql Mqtt Svg)

include(GNUInstallDirs)

set(MQTT_PLUS_LUA_VERSION "5.5.0" CACHE STRING "Lua source release to build")
set(MQTT_PLUS_LUA_URL "https://www.lua.org/ftp/lua-${MQTT_PLUS_LUA_VERSION}.tar.gz")
set(MQTT_PLUS_LUA_SHA256 "57ccc32bbbd005cab75bcc52444052535af691789dba2b9016d5c50640d68b3d")

include(FetchContent)
FetchContent_Declare(
    lua
    URL "${MQTT_PLUS_LUA_URL}"
    URL_HASH SHA256=${MQTT_PLUS_LUA_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(lua)

file(
    GLOB MQTT_PLUS_LUA_SOURCES
    CONFIGURE_DEPENDS
    "${lua_SOURCE_DIR}/src/*.c"
)
list(
    REMOVE_ITEM MQTT_PLUS_LUA_SOURCES
    "${lua_SOURCE_DIR}/src/lua.c"
    "${lua_SOURCE_DIR}/src/luac.c"
    "${lua_SOURCE_DIR}/src/onelua.c"
)

add_library(mqtt_plus_lua STATIC ${MQTT_PLUS_LUA_SOURCES})
add_library(MqttPlus::Lua ALIAS mqtt_plus_lua)
target_include_directories(mqtt_plus_lua PUBLIC "${lua_SOURCE_DIR}/src")
set_target_properties(
    mqtt_plus_lua
    PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)

if(APPLE)
    target_compile_definitions(mqtt_plus_lua PRIVATE LUA_USE_MACOSX)
elseif(UNIX)
    target_compile_definitions(mqtt_plus_lua PRIVATE LUA_USE_LINUX)
    target_link_libraries(mqtt_plus_lua PUBLIC m ${CMAKE_DL_LIBS})
endif()

qt_standard_project_setup(REQUIRES 6.11)

qt_add_executable(mqtt_plus_app
    src/main.cpp
    src/appcontroller.cpp
    src/appcontroller.h
    src/eventrenderer.cpp
    src/eventrenderer.h
    src/historystore.cpp
    src/historystore.h
    src/luarunner.cpp
    src/luarunner.h
    src/payloadcodec.cpp
    src/payloadcodec.h
    src/scriptstore.cpp
    src/scriptstore.h
    src/sessionconfig.cpp
    src/sessionconfig.h
)

qt_add_qml_module(mqtt_plus_app
    URI MqttPlusApp
    VERSION 1.0
    QML_FILES
        qml/components/AppBadge.qml
        qml/components/AppButton.qml
        qml/components/AppCheckBox.qml
        qml/components/AppComboBox.qml
        qml/components/AppDivider.qml
        qml/components/AppIconButton.qml
        qml/components/AppPanel.qml
        qml/components/AppSectionHeader.qml
        qml/components/AppSpinner.qml
        qml/components/AppStatusBadge.qml
        qml/components/AppTextArea.qml
        qml/components/AppTextField.qml
        qml/AppUi.qml
        qml/dialogs/AddSubscriptionDialog.qml
        qml/dialogs/ScriptLibraryWindow.qml
        qml/Main.qml
        qml/panels/SessionActivityPanel.qml
        qml/panels/SessionOverviewPanel.qml
        qml/panels/SessionSidebar.qml
        qml/panels/ScriptWorkspacePanel.qml
        qml/panels/SubscriptionsPanel.qml
        qml/dialogs/SessionEditorDialog.qml
    RESOURCES
        qml/icons/material/chevron-down.svg
        qml/icons/material/chevron-up.svg
        qml/icons/material/check.svg
        qml/icons/material/contrast.svg
        qml/icons/material/dark-mode.svg
        qml/icons/material/edit.svg
        qml/icons/material/light-mode.svg
        qml/icons/material/link.svg
        qml/icons/material/pause.svg
        qml/icons/material/play.svg
        qml/icons/material/power.svg
        qml/icons/material/script-development.svg
        qml/icons/material/send.svg
        qml/icons/material/delete.svg
        qml/icons/material/xmark.svg
        qml/icons/material/plus.svg
)

qt_add_resources(mqtt_plus_app app_icons
    PREFIX "/"
    FILES
        assets/icons/app-icon.png
)

target_link_libraries(mqtt_plus_app
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Mqtt
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Sql
        Qt6::Svg
        MqttPlus::Lua
)

if(APPLE)
    set(MQTT_PLUS_MACOS_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/app-icon.icns")
    target_sources(mqtt_plus_app PRIVATE "${MQTT_PLUS_MACOS_ICON}")
    set_source_files_properties(
        "${MQTT_PLUS_MACOS_ICON}"
        PROPERTIES
            MACOSX_PACKAGE_LOCATION Resources
    )
endif()

set_target_properties(mqtt_plus_app PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_ICON_FILE app-icon.icns
    WIN32_EXECUTABLE TRUE
)

install(TARGETS mqtt_plus_app
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_qml_app_script(
    TARGET mqtt_plus_app
    INCLUDE_PLUGINS qsqlite
    EXCLUDE_PLUGINS qsqlodbc qsqlpsql
    OUTPUT_SCRIPT mqtt_plus_qt_deploy_script
)
install(SCRIPT ${mqtt_plus_qt_deploy_script})

set(CPACK_PACKAGE_NAME "mqtt-plus")
set(CPACK_PACKAGE_VENDOR "mqtt-plus")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Qt/QML desktop MQTT client")

if(WIN32)
    set(CPACK_GENERATOR "ZIP")
    set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-windows-x64")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-macos-${CMAKE_SYSTEM_PROCESSOR}")
elseif(UNIX)
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-linux-${CMAKE_SYSTEM_PROCESSOR}")
endif()

include(CPack)
