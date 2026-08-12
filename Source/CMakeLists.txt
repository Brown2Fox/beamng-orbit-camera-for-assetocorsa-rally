set(TARGET BeamNGOrbitCamera)

set(BEAMNG_ORBIT_CAMERA_SOURCES
    dllmain.cpp
    BeamNGOrbitCameraMod.cpp
    CameraHelpers/Collision.cpp
    CameraHelpers/Input.cpp
    CameraHelpers/BodyGeometry.cpp
    Config/Config.cpp
    Core/MiniMath.cpp
    Core/UnrealReflection.cpp
)

if(BEAMNG_ORBIT_CAMERA_ENABLE_DIAGNOSTICS)
    list(APPEND BEAMNG_ORBIT_CAMERA_SOURCES
        Diagnostics/Diagnostics.cpp
    )
endif()

add_library(${TARGET} SHARED ${BEAMNG_ORBIT_CAMERA_SOURCES})

target_include_directories(${TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(${TARGET} PRIVATE cxx_std_20)

target_link_libraries(${TARGET} PUBLIC UE4SS)

if(BEAMNG_ORBIT_CAMERA_ENABLE_DIAGNOSTICS)
    target_compile_definitions(
        ${TARGET}
        PRIVATE BEAMNG_ORBIT_CAMERA_DIAGNOSTICS=1
    )
else()
    target_compile_definitions(
        ${TARGET}
        PRIVATE BEAMNG_ORBIT_CAMERA_DIAGNOSTICS=0
    )
endif()
