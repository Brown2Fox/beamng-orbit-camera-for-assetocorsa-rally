#pragma once

#include <cstdint>

#include "Core/MiniMath.hpp"

#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>

using namespace RC;
using namespace RC::Unreal;

namespace MiniMath = BeamNGOrbitCamera::MiniMath;

#ifndef BEAMNG_ORBIT_CAMERA_DIAGNOSTICS
#define BEAMNG_ORBIT_CAMERA_DIAGNOSTICS 0
#endif

struct FStructAccess
{
    FStructProperty* Property{};
    UStruct* Type{};
    void* Value{};
};

struct FOrbitInput
{
    double YawStepRad{};
    double PitchStepRad{};
    double ZoomStep{};
    double TargetHeightStepCm{};
    bool bRecenter{};
    bool bRecenterKeep{};
};

struct FBodyGeometry
{
    MiniMath::FVector Center{};
    MiniMath::FVector Extent{};
    MiniMath::FVector Forward{};
    MiniMath::FVector Up{};
};

struct FRawScriptArray
{
    void* Data{};
    int32_t Num{};
    int32_t Max{};
};
