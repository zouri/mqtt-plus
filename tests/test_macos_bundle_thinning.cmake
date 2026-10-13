cmake_minimum_required(VERSION 3.29)
include("${SOURCE_DIR}/cmake/ThinMacosBundle.cmake")

if(FAILURE_CASE)
    mqtt_plus_thin_macos_bundle("${TEST_DIR}/thin" "x86_64")
    return()
endif()

find_program(clang clang REQUIRED)
find_program(lipo lipo REQUIRED)
file(REMOVE_RECURSE "${TEST_DIR}")
file(MAKE_DIRECTORY "${TEST_DIR}/thin" "${TEST_DIR}/universal")
file(WRITE "${TEST_DIR}/fixture.c" "int fixture(void) { return 42; }\n")
foreach(arch arm64 x86_64)
    execute_process(COMMAND "${clang}" -arch "${arch}" -c
        "${TEST_DIR}/fixture.c" -o "${TEST_DIR}/${arch}.o"
        COMMAND_ERROR_IS_FATAL ANY)
endforeach()
execute_process(COMMAND "${lipo}" -create "${TEST_DIR}/arm64.o" "${TEST_DIR}/x86_64.o"
    -output "${TEST_DIR}/universal/fixture" COMMAND_ERROR_IS_FATAL ANY)
file(COPY "${TEST_DIR}/universal/fixture" DESTINATION "${TEST_DIR}/thin")
file(COPY "${TEST_DIR}/universal/fixture" DESTINATION "${TEST_DIR}/intel")
file(WRITE "${TEST_DIR}/thin/resource.txt" "unchanged resource\n")
file(CREATE_LINK fixture "${TEST_DIR}/thin/link" SYMBOLIC)

file(SHA256 "${TEST_DIR}/universal/fixture" universal_hash)
mqtt_plus_thin_macos_bundle("${TEST_DIR}/universal" "arm64;x86_64")
file(SHA256 "${TEST_DIR}/universal/fixture" unchanged_hash)
if(NOT universal_hash STREQUAL unchanged_hash)
    message(FATAL_ERROR "Universal build was modified")
endif()

mqtt_plus_thin_macos_bundle("${TEST_DIR}/intel" "x86_64")
execute_process(COMMAND "${lipo}" -archs "${TEST_DIR}/intel/fixture"
    OUTPUT_VARIABLE intel_archs OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
if(NOT intel_archs STREQUAL "x86_64")
    message(FATAL_ERROR "Intel bundle retained an unused architecture")
endif()

mqtt_plus_thin_macos_bundle("${TEST_DIR}/thin" "arm64")
mqtt_plus_thin_macos_bundle("${TEST_DIR}/thin" "arm64")
execute_process(COMMAND "${lipo}" -archs "${TEST_DIR}/thin/fixture"
    OUTPUT_VARIABLE archs OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
file(READ "${TEST_DIR}/thin/resource.txt" resource)
if(NOT archs STREQUAL "arm64" OR NOT resource STREQUAL "unchanged resource\n"
        OR NOT IS_SYMLINK "${TEST_DIR}/thin/link")
    message(FATAL_ERROR "Thinning did not preserve the selected architecture, resources or symlinks")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${SOURCE_DIR}"
    "-DTEST_DIR=${TEST_DIR}" -DFAILURE_CASE=ON -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(result EQUAL 0 OR NOT error MATCHES "missing architecture x86_64")
    message(FATAL_ERROR "Missing architecture did not fail safely: ${output}${error}")
endif()
