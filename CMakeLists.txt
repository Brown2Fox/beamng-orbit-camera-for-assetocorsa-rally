cmake_minimum_required(VERSION 3.22)
project(BeamNGOrbitCamera LANGUAGES CXX)

set(
    BEAMNG_ORBIT_CAMERA_UE4SS_REVISION
    "1c1a1497f942c707f47ba668db75b25e86f6c08a"
)

option(
    BEAMNG_ORBIT_CAMERA_ENABLE_DIAGNOSTICS
    "Compile development diagnostics"
    OFF
)

set(
    BEAMNG_ORBIT_CAMERA_UE4SS_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/RE-UE4SS"
    CACHE PATH
    "Path to the RE-UE4SS checkout used to build the mod"
)

if(NOT EXISTS "${BEAMNG_ORBIT_CAMERA_UE4SS_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "RE-UE4SS not found at: ${BEAMNG_ORBIT_CAMERA_UE4SS_DIR}\n"
        "Clone/check out the matching UE4SS revision into RE-UE4SS or pass "
        "-DBEAMNG_ORBIT_CAMERA_UE4SS_DIR=<path>."
    )
endif()

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${BEAMNG_ORBIT_CAMERA_UE4SS_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
        WORKING_DIRECTORY "${BEAMNG_ORBIT_CAMERA_UE4SS_DIR}"
        RESULT_VARIABLE BEAMNG_ORBIT_CAMERA_UE4SS_REVISION_RESULT
        OUTPUT_VARIABLE BEAMNG_ORBIT_CAMERA_UE4SS_ACTUAL_REVISION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if(NOT BEAMNG_ORBIT_CAMERA_UE4SS_REVISION_RESULT EQUAL 0)
        message(FATAL_ERROR "Could not determine the RE-UE4SS revision.")
    elseif(NOT BEAMNG_ORBIT_CAMERA_UE4SS_ACTUAL_REVISION STREQUAL BEAMNG_ORBIT_CAMERA_UE4SS_REVISION)
        message(FATAL_ERROR
            "Unsupported RE-UE4SS revision: ${BEAMNG_ORBIT_CAMERA_UE4SS_ACTUAL_REVISION}\n"
            "Required revision: ${BEAMNG_ORBIT_CAMERA_UE4SS_REVISION}"
        )
    endif()
else()
    message(WARNING
        "RE-UE4SS Git metadata is unavailable; required revision "
        "${BEAMNG_ORBIT_CAMERA_UE4SS_REVISION} cannot be verified."
    )
endif()

add_subdirectory(
    "${BEAMNG_ORBIT_CAMERA_UE4SS_DIR}"
    "${CMAKE_BINARY_DIR}/RE-UE4SS"
)

add_subdirectory(Source)
