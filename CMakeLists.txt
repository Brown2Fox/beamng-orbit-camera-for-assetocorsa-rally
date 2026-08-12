cmake_minimum_required(VERSION 3.22)
project(BeamNGOrbitCamera LANGUAGES CXX)

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

add_subdirectory(
    "${BEAMNG_ORBIT_CAMERA_UE4SS_DIR}"
    "${CMAKE_BINARY_DIR}/RE-UE4SS"
)

add_subdirectory(Source)
