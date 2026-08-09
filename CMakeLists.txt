cmake_minimum_required(VERSION 3.29)

project(mqtt-plus VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.11 REQUIRED COMPONENTS Concurrent Core Gui Network Qml Quick QuickControls2 Sql Mqtt Svg Test LinguistTools)

include(GNUInstallDirs)
include(CTest)

set(MQTT_PLUS_LUA_VERSION "5.5.0" CACHE STRING "Lua source release to build")
set(MQTT_PLUS_LUA_URL "https://www.lua.org/ftp/lua-${MQTT_PLUS_LUA_VERSION}.tar.gz")
set(MQTT_PLUS_LUA_SHA256 "57ccc32bbbd005cab75bcc52444052535af691789dba2b9016d5c50640d68b3d")

include(FetchContent)
set(
    MQTT_PLUS_FETCHCONTENT_BASE_DIR
    "${CMAKE_SOURCE_DIR}/build/fetchcontent"
    CACHE PATH
    "Shared source and download cache for FetchContent dependencies"
)
set(FETCHCONTENT_BASE_DIR "${MQTT_PLUS_FETCHCONTENT_BASE_DIR}")

function(mqtt_plus_reuse_cached_dependency dependency marker)
    string(TOUPPER "${dependency}" dependency_upper)
    set(source_override "FETCHCONTENT_SOURCE_DIR_${dependency_upper}")
    set(source_dir "${MQTT_PLUS_FETCHCONTENT_BASE_DIR}/${dependency}-src")
    if(NOT DEFINED "${source_override}" AND EXISTS "${source_dir}/${marker}")
        set("${source_override}" "${source_dir}" PARENT_SCOPE)
    endif()
endfunction()

set(MQTT_PLUS_BUILD_TESTING "${BUILD_TESTING}")
set(MQTT_PLUS_BUILD_DOC "${BUILD_DOC}")
set(MQTT_PLUS_BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS}")
set(BUILD_TESTING OFF)
set(BUILD_DOC OFF)
mqtt_plus_reuse_cached_dependency(lua "src/lua.c")
FetchContent_Declare(
    lua
    URL "${MQTT_PLUS_LUA_URL}"
    URL_HASH SHA256=${MQTT_PLUS_LUA_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_DIR "${MQTT_PLUS_FETCHCONTENT_BASE_DIR}/lua-src"
    BINARY_DIR "${CMAKE_BINARY_DIR}/_deps/lua-build"
)
FetchContent_MakeAvailable(lua)

mqtt_plus_reuse_cached_dependency(extra_cmake_modules "CMakeLists.txt")
FetchContent_Declare(
    extra_cmake_modules
    URL "https://github.com/KDE/extra-cmake-modules/archive/refs/tags/v6.28.0.tar.gz"
    URL_HASH "SHA256=279e172cfc909b2c22dbe7780a6b9af610c4c6b4067e8c30cfe390df65cbb3de"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_DIR "${MQTT_PLUS_FETCHCONTENT_BASE_DIR}/extra_cmake_modules-src"
    BINARY_DIR "${CMAKE_BINARY_DIR}/_deps/extra_cmake_modules-build"
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(extra_cmake_modules)
set(MQTT_PLUS_ECM_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}/mqtt-plus-ecm")
file(MAKE_DIRECTORY "${MQTT_PLUS_ECM_CONFIG_DIR}")
file(WRITE "${MQTT_PLUS_ECM_CONFIG_DIR}/ECMConfig.cmake"
    "set(ECM_FIND_MODULE_DIR \"${extra_cmake_modules_SOURCE_DIR}/find-modules\")\n"
    "set(ECM_MODULE_DIR \"${extra_cmake_modules_SOURCE_DIR}/modules\")\n"
    "set(ECM_KDE_MODULE_DIR \"${extra_cmake_modules_SOURCE_DIR}/kde-modules\")\n"
    "set(ECM_MODULE_PATH \"\${ECM_MODULE_DIR}\" \"\${ECM_FIND_MODULE_DIR}\" \"\${ECM_KDE_MODULE_DIR}\")\n"
    "include(\"\${ECM_MODULE_DIR}/ECMUseFindModules.cmake\")\n"
)
configure_file(
    "${extra_cmake_modules_BINARY_DIR}/ECMConfigVersion.cmake"
    "${MQTT_PLUS_ECM_CONFIG_DIR}/ECMConfigVersion.cmake"
    COPYONLY
)
set(ECM_DIR "${MQTT_PLUS_ECM_CONFIG_DIR}" CACHE PATH "" FORCE)

set(KSYNTAXHIGHLIGHTING_USE_GUI ON CACHE BOOL "" FORCE)
set(QRC_SYNTAX ON CACHE BOOL "" FORCE)
set(NO_STANDARD_PATHS ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF)
mqtt_plus_reuse_cached_dependency(ksyntaxhighlighting "CMakeLists.txt")
FetchContent_Declare(
    ksyntaxhighlighting
    URL "https://github.com/KDE/syntax-highlighting/archive/refs/tags/v6.28.1.tar.gz"
    URL_HASH "SHA256=a40ce6c08c93dd1673eb7330c9792c9fe326f43e021e11b95474c496692c8ad3"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_DIR "${MQTT_PLUS_FETCHCONTENT_BASE_DIR}/ksyntaxhighlighting-src"
    BINARY_DIR "${CMAKE_BINARY_DIR}/_deps/ksyntaxhighlighting-build"
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(ksyntaxhighlighting)
set(BUILD_TESTING "${MQTT_PLUS_BUILD_TESTING}")
set(BUILD_DOC "${MQTT_PLUS_BUILD_DOC}")
set(BUILD_SHARED_LIBS "${MQTT_PLUS_BUILD_SHARED_LIBS}")

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
set(MQTT_PLUS_APP_LIBRARY_SOURCES ${MQTT_PLUS_APP_SOURCES})
list(REMOVE_ITEM MQTT_PLUS_APP_LIBRARY_SOURCES
    "src/app/main.cpp"
    "src/app/messagestreamprofiledriver.cpp"
    "src/app/messagestreamprofiledriver.h"
    "src/app/windowgeometrymanager.cpp"
    "src/app/windowgeometrymanager.h"
)

qt_add_executable(mqtt_plus_app
    ${MQTT_PLUS_APP_SOURCES}
)

qt_add_translations(mqtt_plus_app
    TS_FILES
        i18n/mqtt_plus_zh_CN.ts
    LUPDATE_OPTIONS
        -no-obsolete
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
        "${CMAKE_CURRENT_SOURCE_DIR}/src/presentation"
)

target_compile_definitions(mqtt_plus_app
    PRIVATE
        $<$<CONFIG:Debug>:QT_QML_DEBUG>
)

target_link_libraries(mqtt_plus_app
    PRIVATE
        Qt6::Core
        Qt6::Concurrent
        Qt6::Gui
        Qt6::Network
        Qt6::Mqtt
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Sql
        Qt6::Svg
        KF6SyntaxHighlighting
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
    function(add_mqtt_plus_qt_test target)
        add_executable(${target}
            ${ARGN}
        )
        target_include_directories(${target}
            PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/src"
        )
        target_link_libraries(${target}
            PRIVATE
                Qt6::Core
                Qt6::Concurrent
                Qt6::Gui
                Qt6::Network
                Qt6::Mqtt
                Qt6::Qml
                Qt6::Quick
                Qt6::QuickControls2
                Qt6::Sql
                Qt6::Svg
                Qt6::Test
                KF6SyntaxHighlighting
                MqttPlus::Lua
        )
        add_test(NAME ${target} COMMAND ${target})
    endfunction()

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

    add_mqtt_plus_qt_test(test_configurationadapters
        tests/test_configurationadapters.cpp
        src/services/configuration/configurationadapters.cpp
    )

    add_mqtt_plus_qt_test(test_configurationtransferservice
        tests/test_configurationtransferservice.cpp
        ${MQTT_PLUS_APP_LIBRARY_SOURCES}
    )

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

    add_mqtt_plus_qt_test(test_draftstore
        tests/test_draftstore.cpp
        src/services/payload/payloadcodec.cpp
        src/services/storage/draftstore.cpp
    )

    add_mqtt_plus_qt_test(test_draftlibraryservice
        tests/test_draftlibraryservice.cpp
        src/services/apputils.cpp
        src/services/payload/payloadcodec.cpp
        src/services/storage/draftstore.cpp
        src/usecases/draftlibraryservice.cpp
    )

    add_mqtt_plus_qt_test(test_drafteditorviewmodel
        tests/test_drafteditorviewmodel.cpp
        src/domain/sessionconfig.cpp
        src/services/payload/payloadcodec.cpp
        src/viewmodels/drafteditorviewmodel.cpp
    )

    add_mqtt_plus_qt_test(test_draftmodels
        tests/test_draftmodels.cpp
        src/models/draftfiltermodel.cpp
        src/models/draftlibrarymodel.cpp
        src/services/payload/payloadcodec.cpp
    )

    add_mqtt_plus_qt_test(test_notificationcentermodel
        tests/test_notificationcentermodel.cpp
        src/models/notificationcentermodel.cpp
    )

    add_executable(test_recenttrafficwindow
        tests/test_recenttrafficwindow.cpp
    )
    target_include_directories(test_recenttrafficwindow
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(test_recenttrafficwindow
        PRIVATE
            Qt6::Core
            Qt6::Test
    )
    add_test(NAME recenttrafficwindow COMMAND test_recenttrafficwindow)

    add_executable(test_messagecapturepolicy
        tests/test_messagecapturepolicy.cpp
        src/services/messaging/messagecapturepolicy.cpp
        src/services/payload/payloadcodec.cpp
    )
    target_include_directories(test_messagecapturepolicy
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(test_messagecapturepolicy
        PRIVATE
            Qt6::Core
            Qt6::Test
    )
    add_test(NAME messagecapturepolicy COMMAND test_messagecapturepolicy)

    add_mqtt_plus_qt_test(test_processorlibrarystore
        tests/test_processorlibrarystore.cpp
        src/services/processors/processorpackagehash.cpp
        src/services/processors/processorlibrarystore.cpp
    )

    add_mqtt_plus_qt_test(test_processorlibrary
        tests/test_processorlibrary.cpp
        src/services/processors/processorlibrary.cpp
        src/services/processors/processorpackagehash.cpp
        src/services/processors/processorlibrarystore.cpp
    )

    add_mqtt_plus_qt_test(test_messageprocessorengine
        tests/test_messageprocessorengine.cpp
        src/services/processors/messageprocessorengine.cpp
        src/services/processors/processorruntimeregistry.cpp
        src/services/processors/processorvaluecodec.cpp
    )

    add_mqtt_plus_qt_test(test_luaruntimeadapter
        tests/test_luaruntimeadapter.cpp
        src/services/processors/messageprocessorengine.cpp
        src/services/processors/processorruntimeregistry.cpp
        src/services/processors/processorvaluecodec.cpp
        src/services/processors/runtimes/luaruntimeadapter.cpp
    )

    add_mqtt_plus_qt_test(test_javascriptruntimeadapter
        tests/test_javascriptruntimeadapter.cpp
        src/services/processors/messageprocessorengine.cpp
        src/services/processors/processorruntimeregistry.cpp
        src/services/processors/processorvaluecodec.cpp
        src/services/processors/runtimes/javascriptruntimeadapter.cpp
    )

    add_mqtt_plus_qt_test(test_sessioneditorviewmodel
        tests/test_sessioneditorviewmodel.cpp
        src/domain/sessionconfig.cpp
        src/viewmodels/sessioneditorviewmodel.cpp
    )

    add_mqtt_plus_qt_test(test_sessionimport
        tests/test_sessionimport.cpp
        ${MQTT_PLUS_APP_LIBRARY_SOURCES}
    )

    add_mqtt_plus_qt_test(test_subscriptioneditorviewmodel
        tests/test_subscriptioneditorviewmodel.cpp
        src/domain/sessionconfig.cpp
        src/viewmodels/subscriptioneditorviewmodel.cpp
    )

    add_mqtt_plus_qt_test(test_sessionlistmodel
        tests/test_sessionlistmodel.cpp
        src/domain/sessionconfig.cpp
        src/models/sessionlistmodel.cpp
        src/services/apputils.cpp
    )

    add_mqtt_plus_qt_test(test_connectionerrortext
        tests/test_connectionerrortext.cpp
        ${MQTT_PLUS_APP_LIBRARY_SOURCES}
    )

    add_mqtt_plus_qt_test(test_eventstreammodel
        tests/test_eventstreammodel.cpp
        src/models/eventstreammodel.cpp
    )

    add_mqtt_plus_qt_test(test_messagefiltermodel
        tests/test_messagefiltermodel.cpp
        src/models/eventstreammodel.cpp
        src/models/messagefiltermodel.cpp
        src/services/payload/payloadcodec.cpp
    )

    add_mqtt_plus_qt_test(test_eventhistoryservice
        tests/test_eventhistoryservice.cpp
        ${MQTT_PLUS_APP_LIBRARY_SOURCES}
    )

    add_mqtt_plus_qt_test(test_historystore
        tests/test_historystore.cpp
        src/services/storage/historystore.cpp
    )

    add_mqtt_plus_qt_test(test_historywriterworker
        tests/test_historywriterworker.cpp
        src/services/storage/historystore.cpp
        src/services/storage/historywriterworker.cpp
    )

    add_mqtt_plus_qt_test(test_messageparseworker
        tests/test_messageparseworker.cpp
        src/services/parsing/messageparseworker.cpp
        src/services/payload/payloadcodec.cpp
        src/services/processors/defaultmessageprocessorengine.cpp
        src/services/processors/messageprocessorengine.cpp
        src/services/processors/processorruntimeregistry.cpp
        src/services/processors/processorvaluecodec.cpp
        src/services/processors/runtimes/javascriptruntimeadapter.cpp
        src/services/processors/runtimes/luaruntimeadapter.cpp
    )

    add_mqtt_plus_qt_test(test_subscriptionlistmodel
        tests/test_subscriptionlistmodel.cpp
        ${MQTT_PLUS_APP_LIBRARY_SOURCES}
    )

    add_mqtt_plus_qt_test(test_subscriptionservice
        tests/test_subscriptionservice.cpp
        ${MQTT_PLUS_APP_LIBRARY_SOURCES}
    )

    add_mqtt_plus_qt_test(test_processorsviewmodel
        tests/test_processorsviewmodel.cpp
        ${MQTT_PLUS_APP_LIBRARY_SOURCES}
    )

    add_mqtt_plus_qt_test(test_codesyntaxhighlighter
        tests/test_codesyntaxhighlighter.cpp
        src/presentation/codesyntaxhighlighter.cpp
    )

    add_mqtt_plus_qt_test(test_settingsviewmodel
        tests/test_settingsviewmodel.cpp
        ${MQTT_PLUS_APP_LIBRARY_SOURCES}
    )

    add_mqtt_plus_qt_test(test_windowgeometrymanager
        tests/test_windowgeometrymanager.cpp
        src/app/windowgeometrymanager.cpp
        src/usecases/preferencescontroller.cpp
    )

    add_mqtt_plus_qt_test(test_logsviewmodel
        tests/test_logsviewmodel.cpp
        ${MQTT_PLUS_APP_LIBRARY_SOURCES}
    )

    add_mqtt_plus_qt_test(test_workbenchviewmodel
        tests/test_workbenchviewmodel.cpp
        ${MQTT_PLUS_APP_LIBRARY_SOURCES}
    )

    add_mqtt_plus_qt_test(test_architecture_boundaries
        tests/test_architecture_boundaries.cpp
    )
    target_compile_definitions(test_architecture_boundaries
        PRIVATE
            MQTT_PLUS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()

include(CPack)
