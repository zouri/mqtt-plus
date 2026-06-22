cmake_minimum_required(VERSION 3.24)

project(mqtt-plus VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2 Sql Mqtt Svg Widgets Test LinguistTools)

include(GNUInstallDirs)
include(CTest)

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

file(
    GLOB_RECURSE MQTT_PLUS_APP_SOURCES
    CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "src/*.cpp"
    "src/*.h"
)
list(SORT MQTT_PLUS_APP_SOURCES)

qt_add_executable(mqtt_plus_app
    ${MQTT_PLUS_APP_SOURCES}
)

qt_add_translations(mqtt_plus_app
    TS_FILES
        i18n/mqtt_plus_zh_CN.ts
    RESOURCE_PREFIX
        "/i18n"
)

file(GLOB_RECURSE QML_FILES
    CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "qml/*.qml"
)
file(GLOB_RECURSE SVG_RESOURCES
    CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "resources/*.svg"
)
list(SORT QML_FILES)
list(SORT SVG_RESOURCES)

qt_add_qml_module(mqtt_plus_app
    URI MqttPlusApp
    VERSION 1.0
    # Qt 6.11 qmlimportscanner currently hangs on the generated module qmldir.
    # Direct qmllint remains available through the generated qmllint targets.
    NO_IMPORT_SCAN
    QML_FILES
        ${QML_FILES}
    RESOURCES
        ${SVG_RESOURCES}
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

if(BUILD_TESTING)
    set(MQTT_PLUS_TEST_APP_SOURCES ${MQTT_PLUS_APP_SOURCES})
    list(FILTER MQTT_PLUS_TEST_APP_SOURCES EXCLUDE REGEX "^src/app/main\\.cpp$")

    add_executable(test_sessionconfig
        tests/test_sessionconfig.cpp
        src/domain/sessionconfig.cpp
    )
    target_include_directories(test_sessionconfig
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(test_sessionconfig
        PRIVATE
            Qt6::Core
            Qt6::Test
    )
    add_test(NAME sessionconfig COMMAND test_sessionconfig)

    add_executable(test_payloadcodec
        tests/test_payloadcodec.cpp
        src/services/payload/payloadcodec.cpp
    )
    target_include_directories(test_payloadcodec
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(test_payloadcodec
        PRIVATE
            Qt6::Core
            Qt6::Test
    )
    add_test(NAME payloadcodec COMMAND test_payloadcodec)

    add_executable(test_eventstreammodel
        tests/test_eventstreammodel.cpp
        src/models/eventstreammodel.cpp
    )
    target_include_directories(test_eventstreammodel
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(test_eventstreammodel
        PRIVATE
            Qt6::Core
            Qt6::Test
    )
    add_test(NAME eventstreammodel COMMAND test_eventstreammodel)

    add_executable(test_appfacadefacades
        tests/test_appfacadefacades.cpp
        ${MQTT_PLUS_TEST_APP_SOURCES}
    )
    target_include_directories(test_appfacadefacades
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(test_appfacadefacades
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Network
            Qt6::Mqtt
            Qt6::Sql
            Qt6::Widgets
            Qt6::Test
            MqttPlus::Lua
    )
    add_test(NAME appfacadefacades COMMAND test_appfacadefacades)
endif()

include(CPack)
