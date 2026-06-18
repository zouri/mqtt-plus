cmake_minimum_required(VERSION 3.24)

project(mqtt-plus VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2 Sql Mqtt Svg Widgets LinguistTools)

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
    src/app/main.cpp
    src/app/appfacade.cpp
    src/app/appfacade.h
    src/app/appfacadeevents.cpp
    src/app/appfacademodels.cpp
    src/app/appfacademqtt.cpp
    src/app/appfacademenus.cpp
    src/app/appfacadepreferences.cpp
    src/app/appfacadescripts.cpp
    src/app/appfacadesessions.cpp
    src/app/appfacadesubscriptions.cpp
    src/app/appfacadetheme.cpp
    src/app/appfacadeutils.cpp
    src/app/appfacadeutils.h
    src/controllers/themecontroller.cpp
    src/controllers/themecontroller.h
    src/controllers/scriptcontroller.cpp
    src/controllers/scriptcontroller.h
    src/controllers/sessioncontroller.cpp
    src/controllers/sessioncontroller.h
    src/controllers/subscriptioncontroller.cpp
    src/controllers/subscriptioncontroller.h
    src/controllers/mqttcontroller.cpp
    src/controllers/mqttcontroller.h
    src/controllers/eventcontroller.cpp
    src/controllers/eventcontroller.h
    src/controllers/languagecontroller.cpp
    src/controllers/languagecontroller.h
    src/controllers/preferencescontroller.cpp
    src/controllers/preferencescontroller.h
    src/domain/script.h
    src/domain/session.h
    src/domain/sessionconfig.cpp
    src/domain/sessionconfig.h
    src/domain/subscription.h
    src/models/eventstreammodel.cpp
    src/models/eventstreammodel.h
    src/models/scriptlibrarymodel.cpp
    src/models/scriptlibrarymodel.h
    src/models/scripttestsamplesmodel.cpp
    src/models/scripttestsamplesmodel.h
    src/models/sessionlistmodel.cpp
    src/models/sessionlistmodel.h
    src/models/subscriptionlistmodel.cpp
    src/models/subscriptionlistmodel.h
    src/presentation/eventrenderer.cpp
    src/presentation/eventrenderer.h
    src/services/payload/payloadcodec.cpp
    src/services/payload/payloadcodec.h
    src/services/scripting/luarunner.cpp
    src/services/scripting/luarunner.h
    src/services/storage/historystore.cpp
    src/services/storage/historystore.h
    src/services/storage/scriptstore.cpp
    src/services/storage/scriptstore.h
    src/services/storage/sessionsettingsstore.cpp
    src/services/storage/sessionsettingsstore.h
)

qt_add_translations(mqtt_plus_app
    TS_FILES
        i18n/mqtt_plus_zh_CN.ts
    RESOURCE_PREFIX
        "/i18n"
)

qt_add_qml_module(mqtt_plus_app
    URI MqttPlusApp
    VERSION 1.0
    # Qt 6.11 qmlimportscanner currently hangs on the generated module qmldir.
    # Direct qmllint remains available through the generated qmllint targets.
    NO_IMPORT_SCAN
    QML_FILES
        qml/components/AppBadge.qml
        qml/components/AppButton.qml
        qml/components/AppCheckBox.qml
        qml/components/AppComboBox.qml
        qml/components/AppDivider.qml
        qml/components/AppIconButton.qml
        qml/components/AppPanel.qml
        qml/components/AppSectionHeader.qml
        qml/components/AppTextArea.qml
        qml/components/AppTextField.qml
        qml/components/AppToolTip.qml
        qml/AppUi.qml
        qml/features/logs/LogsView.qml
        qml/features/scripts/ScriptListPane.qml
        qml/features/scripts/ScriptsView.qml
        qml/features/sessions/SessionEditorDialog.qml
        qml/features/sessions/SessionOverviewPanel.qml
        qml/features/sessions/SessionSidebar.qml
        qml/features/settings/SettingsView.qml
        qml/features/streams/EventStreamView.qml
        qml/features/subscriptions/AddSubscriptionDialog.qml
        qml/features/subscriptions/SubscriptionsPanel.qml
        qml/features/workbench/PublishComposer.qml
        qml/features/workbench/SessionActivityPanel.qml
        qml/features/workbench/WorkbenchView.qml
        qml/Main.qml
    RESOURCES
        qml/icons/chevron-left.svg
        qml/icons/chevron-right.svg
        qml/icons/check.svg
        qml/icons/edit.svg
        qml/icons/logs.svg
        qml/icons/more-horiz.svg
        qml/icons/pause.svg
        qml/icons/play.svg
        qml/icons/plug.svg
        qml/icons/plug-off.svg
        qml/icons/script-development.svg
        qml/icons/send.svg
        qml/icons/settings.svg
        qml/icons/delete.svg
        qml/icons/workbench.svg
        qml/icons/xmark.svg
        qml/icons/plus.svg
)

qt_add_resources(mqtt_plus_app app_icons
    PREFIX "/"
    FILES
        assets/icons/app-icon.png
)

target_include_directories(mqtt_plus_app
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
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
        Qt6::Widgets
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
