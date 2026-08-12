#include "BeamNGOrbitCameraMod.hpp"

#include "Core/MiniMath.hpp"
#include "Core/UnrealReflection.hpp"

#include <algorithm>
#include <cmath>

namespace MiniMath = BeamNGOrbitCamera::MiniMath;
using namespace BeamNGOrbitCamera::MiniMath;
using namespace BeamNGOrbitCamera::Reflection;


namespace
{
    bool WriteCachedVector(FStructProperty* StructProperty, FProperty* XProperty, FProperty* YProperty,
        FProperty* ZProperty, void* Params, const MiniMath::FVector& Value)
    {
        if (!StructProperty || !XProperty || !YProperty || !ZProperty || !Params)
        {
            return false;
        }

        void* StructValue = StructProperty
                ->ContainerPtrToValuePtr<void>(Params);

        if (!StructValue)
        {
            return false;
        }

        return WriteScalar(XProperty, StructValue, Value.X) && WriteScalar(YProperty, StructValue, Value.Y) && WriteScalar(ZProperty, StructValue, Value.Z);
    }
}


void FBeamNGOrbitCameraMod::InvalidateCollisionTraceCache()
{
    bCollisionTraceCacheValid = false;

    KismetSystemLibrary = nullptr;
    LineTraceSingleFunction = nullptr;

    CollisionWorldContextProperty = nullptr;
    CollisionTraceChannelProperty = nullptr;
    CollisionActorsToIgnoreProperty = nullptr;
    CollisionReturnProperty = nullptr;

    CollisionStartProperty = nullptr;
    CollisionEndProperty = nullptr;
    CollisionOutHitProperty = nullptr;

    CollisionStartXProperty = nullptr;
    CollisionStartYProperty = nullptr;
    CollisionStartZProperty = nullptr;

    CollisionEndXProperty = nullptr;
    CollisionEndYProperty = nullptr;
    CollisionEndZProperty = nullptr;

    CollisionHitDistanceProperty = nullptr;

    CollisionTraceParams.clear();
}

bool FBeamNGOrbitCameraMod::InitializeCollisionTraceCache()
{
    bCollisionTraceCacheValid = false;
    CollisionTraceParams.clear();

    if (!KismetSystemLibrary || !LineTraceSingleFunction)
    {
        return false;
    }

    UFunction* Function = LineTraceSingleFunction;

    const int32_t ParamsSize = Function->GetPropertiesSize();

    if (ParamsSize <= 0)
    {
        return false;
    }

    CollisionWorldContextProperty = Function->GetPropertyByNameInChain(STR("WorldContextObject"));

    CollisionTraceChannelProperty = Function->GetPropertyByNameInChain(STR("TraceChannel"));

    CollisionActorsToIgnoreProperty = Function->GetPropertyByNameInChain(STR("ActorsToIgnore"));

    CollisionReturnProperty = Function->GetPropertyByNameInChain(STR("ReturnValue"));

    CollisionStartProperty = CastField<FStructProperty>(Function->GetPropertyByNameInChain(STR("Start")));

    CollisionEndProperty = CastField<FStructProperty>(Function->GetPropertyByNameInChain(STR("End")));

    CollisionOutHitProperty = CastField<FStructProperty>(Function->GetPropertyByNameInChain(STR("OutHit")));

    if (!CollisionWorldContextProperty || !CollisionTraceChannelProperty || !CollisionActorsToIgnoreProperty || !CollisionReturnProperty ||
        !CollisionStartProperty || !CollisionEndProperty || !CollisionOutHitProperty)
    {
        return false;
    }

    UStruct* StartType = CollisionStartProperty->GetStruct();

    UStruct* EndType = CollisionEndProperty->GetStruct();

    UStruct* HitType = CollisionOutHitProperty->GetStruct();

    if (!StartType || !EndType || !HitType)
    {
        return false;
    }

    CollisionStartXProperty = StartType->GetPropertyByNameInChain(STR("X"));
    CollisionStartYProperty = StartType->GetPropertyByNameInChain(STR("Y"));
    CollisionStartZProperty = StartType->GetPropertyByNameInChain(STR("Z"));

    CollisionEndXProperty = EndType->GetPropertyByNameInChain(STR("X"));
    CollisionEndYProperty = EndType->GetPropertyByNameInChain(STR("Y"));
    CollisionEndZProperty = EndType->GetPropertyByNameInChain(STR("Z"));

    CollisionHitDistanceProperty = HitType->GetPropertyByNameInChain(STR("Distance"));

    if (!CollisionStartXProperty || !CollisionStartYProperty || !CollisionStartZProperty || !CollisionEndXProperty ||
        !CollisionEndYProperty || !CollisionEndZProperty || !CollisionHitDistanceProperty)
    {
        return false;
    }

    CollisionTraceParams.assign(static_cast<size_t>(ParamsSize), uint8_t{0});

    bCollisionTraceCacheValid = true;

    return true;
}

void FBeamNGOrbitCameraMod::ResetCollisionState()
{
    bCollisionUseRaycast = true;
    bCollisionHasLastNearClipCenter = false;
    bCollisionDistanceValid = false;
    bCollisionHitThisFrame = false;
    CollisionTraceCountThisFrame = 0;
    CollisionLastNearClipCenter = {};
    CollisionLastDistanceCm = 0.0;
    CollisionDesiredDistanceCm = 0.0;
    CollisionAppliedDistanceCm = 0.0;
}

bool FBeamNGOrbitCameraMod::TraceCameraChannel(UObject* WorldContextObject, const MiniMath::FVector& Start, const MiniMath::FVector& End,
    double& OutHitDistanceCm)
{
    OutHitDistanceCm = 0.0;

    if (!bCollisionTraceCacheValid || !KismetSystemLibrary || !LineTraceSingleFunction || !WorldContextObject || CollisionTraceParams.empty())
    {
        if (!bLoggedCollisionTraceUnavailable)
        {
            bLoggedCollisionTraceUnavailable = true;
            Output::send<LogLevel::Warning>(STR("[BeamNGOrbitCamera] Collision unavailable: cached LineTraceSingle Camera-channel layout is not valid.\n"));
        }
        return false;
    }

    std::fill(CollisionTraceParams.begin(), CollisionTraceParams.end(), uint8_t{0});

    void* Params = CollisionTraceParams.data();

    UObject** WorldContextValue = CollisionWorldContextProperty
            ->ContainerPtrToValuePtr<UObject*>(Params);

    uint8_t* TraceChannelValue = CollisionTraceChannelProperty
            ->ContainerPtrToValuePtr<uint8_t>(Params);

    if (!WorldContextValue || !TraceChannelValue)
    {
        return false;
    }

    *WorldContextValue =
        WorldContextObject;

    // ETraceTypeQuery is project-facing, but Unreal's two built-in trace
    // response channels are Visibility and Camera. TraceTypeQuery2 (value 1)
    // is the built-in Camera query in the engine collision profile.
    constexpr uint8_t CameraTraceTypeQuery = 1;
    *TraceChannelValue =
        CameraTraceTypeQuery;

    if (!WriteCachedVector(CollisionStartProperty, CollisionStartXProperty, CollisionStartYProperty,
            CollisionStartZProperty, Params, Start) || !WriteCachedVector(
            CollisionEndProperty, CollisionEndXProperty, CollisionEndYProperty, CollisionEndZProperty, Params, End))
    {
        return false;
    }

    // The collision function is called with the live vehicle actor as its
    // world context. Ignore it explicitly so Camera-channel responses on the
    // player's own vehicle cannot retract the orbit camera.
    UObject* IgnoredActors[1]{
        WorldContextObject
    };

    FRawScriptArray* ActorsToIgnore = CollisionActorsToIgnoreProperty
            ->ContainerPtrToValuePtr<FRawScriptArray>(Params);

    if (!ActorsToIgnore)
    {
        return false;
    }

    ActorsToIgnore->Data = IgnoredActors;
    ActorsToIgnore->Num = 1;
    ActorsToIgnore->Max = 1;

    ++CollisionTraceCountThisFrame;

    KismetSystemLibrary->ProcessEvent(LineTraceSingleFunction, Params);

    uint8_t* ReturnValue = CollisionReturnProperty
            ->ContainerPtrToValuePtr<uint8_t>(Params);

    if (!ReturnValue ||
        *ReturnValue == 0)
    {
        return false;
    }

    void* HitValue = CollisionOutHitProperty
            ->ContainerPtrToValuePtr<void>(Params);

    if (!HitValue)
    {
        return false;
    }

    double HitDistanceCm = 0.0;
    if (!ReadScalar(CollisionHitDistanceProperty, HitValue, HitDistanceCm))
    {
        return false;
    }

    const double RayLengthCm = Length(End - Start);

    if (!std::isfinite(HitDistanceCm) || HitDistanceCm < 0.0 || HitDistanceCm > RayLengthCm + 5.0)
    {
        return false;
    }

#if BEAMNG_ORBIT_CAMERA_DIAGNOSTICS
    // Keep a lightweight probe for pathological near-zero Camera-channel hits
    // while this trace mode is being validated across ACR tracks.
    if (HitDistanceCm <= CollisionMinHitDistanceCm)
    {
        static ULONGLONG LastDiagnosticTimeMs = 0;
        const ULONGLONG CurrentTimeMs = GetTickCount64();

        if (CurrentTimeMs - LastDiagnosticTimeMs >= 1000)
        {
            LastDiagnosticTimeMs = CurrentTimeMs;

            Output::send<LogLevel::Verbose>(STR("[BeamNGOrbitCamera] CAMERA CHANNEL NEAR HIT: distance={:.2f}cm\n"), HitDistanceCm);
        }
    }
#endif

    OutHitDistanceCm = HitDistanceCm;

    return true;
}


bool FBeamNGOrbitCameraMod::IsObstacleInFrontOfCamera(UObject* WorldContextObject, const MiniMath::FVector (&RayDestinations)[4])
{
    double HitDistanceCm = 0.0;

    // Detect the near-clip plane crossing through geometry between frames.
    if (bCollisionHasLastNearClipCenter)
    {
        const MiniMath::FVector SweepDestination = RayDestinations[0];

        if (Length(SweepDestination - CollisionLastNearClipCenter) > 0.01 && TraceCameraChannel(
                WorldContextObject, CollisionLastNearClipCenter, SweepDestination, HitDistanceCm))
        {
            return true;
        }
    }

    // In idle mode only the four short edges of the current near-clip
    // rectangle are tested. A hit re-enables the full four parallel rays.
    for (int32_t Index = 0; Index < 4; ++Index)
    {
        const MiniMath::FVector& EdgeStart = RayDestinations[Index];

        const MiniMath::FVector& EdgeEnd = RayDestinations[(Index + 1) % 4];

        if (Length(EdgeEnd - EdgeStart) > 0.01 && TraceCameraChannel(WorldContextObject, EdgeStart, EdgeEnd, HitDistanceCm))
        {
            return true;
        }
    }

    return false;
}

MiniMath::FVector FBeamNGOrbitCameraMod::ApplyCameraCollision(UObject* WorldContextObject,
    const MiniMath::FVector& TargetPos, const MiniMath::FVector& DesiredCameraPosition,
    const MiniMath::FVector& DesiredCameraDirection, double DeltaTimeSeconds)
{
    CollisionTraceCountThisFrame = 0;

    const MiniMath::FVector CollisionDirection = DesiredCameraPosition - TargetPos;

    const double DesiredDistanceCm = Length(CollisionDirection);

    CollisionDesiredDistanceCm = DesiredDistanceCm;
    CollisionAppliedDistanceCm = DesiredDistanceCm;
    bCollisionHitThisFrame = false;

    if (DesiredDistanceCm < 0.001)
    {
        ResetCollisionState();
        return DesiredCameraPosition;
    }

    const MiniMath::FVector CameraDirection = Normalized(DesiredCameraDirection, Normalized(
                TargetPos - DesiredCameraPosition, MiniMath::FVector{1.0, 0.0, 0.0}));

    MiniMath::FVector CameraRight = Cross(WorldUp(), CameraDirection);

    if (Length(CameraRight) < 0.0001)
    {
        CameraRight = MiniMath::FVector{1.0, 0.0, 0.0};
    }
    else
    {
        CameraRight = Normalized(CameraRight, MiniMath::FVector{1.0, 0.0, 0.0});
    }

    const MiniMath::FVector CameraUp = Normalized(Cross(CameraDirection, CameraRight), WorldUp());

    const MiniMath::FVector RightOffset = CameraRight * CollisionNearClipHalfWidthCm;

    const MiniMath::FVector UpOffset = CameraUp * CollisionNearClipHalfHeightCm;

    // COLLISION_ASSUMED_NEAR_CLIP_DISTANCE is zero in the source camera,
    // so the near-clip center is the desired camera position itself.
    const MiniMath::FVector NearClipCenter = DesiredCameraPosition;

    const MiniMath::FVector RayDestinations[4] = {
        NearClipCenter + UpOffset + RightOffset, NearClipCenter - UpOffset + RightOffset,
        NearClipCenter - UpOffset - RightOffset, NearClipCenter + UpOffset - RightOffset
    };

    if (!bCollisionUseRaycast && IsObstacleInFrontOfCamera(WorldContextObject, RayDestinations))
    {
        bCollisionUseRaycast = true;
    }

    double ClosestHitDistanceCm = DesiredDistanceCm;

    bool bHitRegistered = false;

    if (bCollisionUseRaycast)
    {
        const MiniMath::FVector CollisionDirectionNormalized = CollisionDirection / DesiredDistanceCm;

        for (const MiniMath::FVector& RayDestination :
             RayDestinations)
        {
            const MiniMath::FVector RayStart = RayDestination - CollisionDirection;

            // BeamNG shortens subsequent rays to the closest hit already
            // found, because anything farther away cannot affect the result.
            const MiniMath::FVector RayEnd = RayStart + CollisionDirectionNormalized * ClosestHitDistanceCm;

            double HitDistanceCm = 0.0;
            if (TraceCameraChannel(WorldContextObject, RayStart, RayEnd, HitDistanceCm))
            {
                bHitRegistered = true;

                bCollisionHitThisFrame = true;

                ClosestHitDistanceCm = std::min(ClosestHitDistanceCm, HitDistanceCm);
            }
        }

        ClosestHitDistanceCm = std::max(ClosestHitDistanceCm, CollisionMinHitDistanceCm);
    }

    // When the full rays see no obstacle, switch to the much shorter
    // near-clip edge tests until geometry approaches the camera plane.
    if (!bHitRegistered)
    {
        bCollisionUseRaycast = false;
    }

    ClosestHitDistanceCm = std::min(ClosestHitDistanceCm, DesiredDistanceCm);

    if (!bCollisionDistanceValid)
    {
        CollisionLastDistanceCm = ClosestHitDistanceCm;

        bCollisionDistanceValid = true;
    }
    else
    {
        const double DestinationDifferenceCm = ClosestHitDistanceCm - CollisionLastDistanceCm;

        if (DestinationDifferenceCm < 0.0)
        {
            // Retraction is immediate so the camera cannot lag through
            // static geometry.
            CollisionLastDistanceCm = ClosestHitDistanceCm;
        }
        else
        {
            const double ReleaseAlpha = 1.0 - std::exp(-CollisionReleaseRate * std::max(DeltaTimeSeconds, 0.0));

            CollisionLastDistanceCm += DestinationDifferenceCm * ReleaseAlpha;
        }
    }

    CollisionLastDistanceCm = std::clamp(CollisionLastDistanceCm, std::min(CollisionMinHitDistanceCm, DesiredDistanceCm), DesiredDistanceCm);

    CollisionLastNearClipCenter = NearClipCenter;

    bCollisionHasLastNearClipCenter = true;

    CollisionAppliedDistanceCm = CollisionLastDistanceCm;

    return TargetPos + CollisionDirection * (CollisionLastDistanceCm / DesiredDistanceCm);
}

