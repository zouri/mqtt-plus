set(app_sources ${MQTT_PLUS_APP_ONLY_SOURCES})
list(TRANSFORM app_sources PREPEND "${PROJECT_SOURCE_DIR}/")

option(MQTT_PLUS_ENABLE_PROFILING "Enable isolated synthetic workloads without QML debugging" OFF)

qt_add_executable(mqtt_plus_app MANUAL_FINALIZATION ${app_sources})
if(WIN32)
    find_program(MQTT_PLUS_NPM NAMES npm.cmd REQUIRED)
    # Select the Windows launcher, not npm's extensionless POSIX shell script.
    set(editor_npm_command cmd /c call "${MQTT_PLUS_NPM}")
else()
    find_program(MQTT_PLUS_NPM npm REQUIRED)
    set(editor_npm_command "${MQTT_PLUS_NPM}")
endif()
set(editor_source "${PROJECT_SOURCE_DIR}/web/processor-editor")
set_property(TARGET mqtt_plus_app APPEND PROPERTY QT_EXCLUDE_SOURCES_FROM_TRANSLATION
    "${editor_source}/dist/*")
file(GLOB_RECURSE editor_inputs CONFIGURE_DEPENDS "${editor_source}/src/*" "${editor_source}/test/*")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${editor_inputs}
    "${editor_source}/package-lock.json" "${editor_source}/package.json"
    "${editor_source}/index.html" "${editor_source}/tsconfig.json" "${editor_source}/vite.config.ts")
file(SHA256 "${editor_source}/package-lock.json" editor_lock_hash)
set(editor_lock_stamp "${editor_source}/node_modules/.mqtt-plus-lock-hash")
set(editor_installed_hash "")
if(EXISTS "${editor_lock_stamp}")
    file(READ "${editor_lock_stamp}" editor_installed_hash)
endif()
if(NOT EXISTS "${editor_source}/node_modules/.package-lock.json" OR NOT editor_installed_hash STREQUAL editor_lock_hash)
    execute_process(COMMAND ${editor_npm_command} ci --no-audit --no-fund
        WORKING_DIRECTORY "${editor_source}" COMMAND_ERROR_IS_FATAL ANY)
    file(WRITE "${editor_lock_stamp}" "${editor_lock_hash}")
endif()
execute_process(COMMAND ${editor_npm_command} test
    WORKING_DIRECTORY "${editor_source}" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND ${editor_npm_command} run build
    WORKING_DIRECTORY "${editor_source}" COMMAND_ERROR_IS_FATAL ANY)
file(GLOB_RECURSE editor_resources "${editor_source}/dist/*")
qt_add_resources(mqtt_plus_app processor_editor
    PREFIX "/processor-editor" BASE "${editor_source}/dist" FILES ${editor_resources})
set_target_properties(mqtt_plus_app PROPERTIES
    OUTPUT_NAME "mqtt-plus"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
)

qt_add_translations(mqtt_plus_app
    SOURCE_TARGETS
        mqtt_plus_app
        mqtt_plus_core
    TS_FILES
        "${PROJECT_SOURCE_DIR}/i18n/mqtt_plus_zh_CN.ts"
    LUPDATE_OPTIONS
        -no-obsolete
    RESOURCE_PREFIX
        "/i18n"
)

file(GLOB_RECURSE QML_FILES
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/qml/*.qml"
)
file(GLOB_RECURSE SVG_RESOURCES
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/resources/*.svg"
)
list(SORT QML_FILES)
list(SORT SVG_RESOURCES)

# Preserve existing qrc URLs after moving the target into this directory.
foreach(resource IN LISTS QML_FILES SVG_RESOURCES)
    file(RELATIVE_PATH alias "${PROJECT_SOURCE_DIR}" "${resource}")
    set_source_files_properties("${resource}" PROPERTIES QT_RESOURCE_ALIAS "${alias}")
endforeach()

qt_add_qml_module(mqtt_plus_app
    URI MqttPlusApp
    VERSION 1.0
    QML_FILES
        ${QML_FILES}
    RESOURCES
        ${SVG_RESOURCES}
)

qt_add_resources(mqtt_plus_app app_icons
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}"
    FILES
        "${PROJECT_SOURCE_DIR}/assets/icons/app-icon.png"
        "${PROJECT_SOURCE_DIR}/assets/icons/app-icon-theme-mask.png"
)

target_include_directories(mqtt_plus_app
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
        "${PROJECT_SOURCE_DIR}/src/presentation"
)

target_compile_definitions(mqtt_plus_app
    PRIVATE
        $<$<CONFIG:Debug>:QT_QML_DEBUG>
        $<$<BOOL:${MQTT_PLUS_ENABLE_PROFILING}>:MQTT_PLUS_ENABLE_PROFILING>
        MQTT_PLUS_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(mqtt_plus_app
    PRIVATE
        MqttPlus::Core
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
        Qt6::WebSockets
        MqttPlus::Lua
)

if(APPLE)
    set(MQTT_PLUS_MACOS_ICON "${PROJECT_SOURCE_DIR}/assets/icons/app-icon.icns")
    target_sources(mqtt_plus_app PRIVATE "${MQTT_PLUS_MACOS_ICON}")
    set_source_files_properties(
        "${MQTT_PLUS_MACOS_ICON}"
        PROPERTIES
            MACOSX_PACKAGE_LOCATION Resources
    )
elseif(WIN32)
    set(MQTT_PLUS_WINDOWS_ICON "${PROJECT_SOURCE_DIR}/assets/icons/app-icon.ico")
    set(MQTT_PLUS_WINDOWS_ICON "${MQTT_PLUS_WINDOWS_ICON}" PARENT_SCOPE)
    configure_file(
        "${PROJECT_SOURCE_DIR}/packaging/windows/app-icon.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/mqtt-plus-app-icon.rc"
        @ONLY
    )
    target_sources(
        mqtt_plus_app
        PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/mqtt-plus-app-icon.rc"
    )
endif()

set_target_properties(mqtt_plus_app PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_ICON_FILE app-icon.icns
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    WIN32_EXECUTABLE TRUE
)

qt_finalize_executable(mqtt_plus_app)
