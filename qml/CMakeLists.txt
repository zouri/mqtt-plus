set(app_sources ${MQTT_PLUS_APP_ONLY_SOURCES})
list(TRANSFORM app_sources PREPEND "${PROJECT_SOURCE_DIR}/")

qt_add_executable(mqtt_plus_app MANUAL_FINALIZATION ${app_sources})
set_target_properties(mqtt_plus_app PROPERTIES
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
        KF6SyntaxHighlighting
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
