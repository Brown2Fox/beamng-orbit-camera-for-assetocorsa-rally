#pragma once

#define NOMINMAX
#include <Windows.h>
#include <Xinput.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <vector>

#include <Mod/CppUserModBase.hpp>
#include <DynamicOutput/DynamicOutput.hpp>
#include <Helpers/String.hpp>

#include <Unreal/UObject.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/UObjectArray.hpp>
#include <Unreal/UEngine.hpp>
#include <Unreal/Hooks/Hooks.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>

#ifndef BEAMNG_ORBIT_CAMERA_DIAGNOSTICS
#define BEAMNG_ORBIT_CAMERA_DIAGNOSTICS 0
#endif

#include "Core/CameraTypes.hpp"
#include "Core/MiniMath.hpp"
#include "Config/Config.hpp"

using namespace RC;
using namespace RC::Unreal;

/*
    Camera behavior adapted from:
      Brown2Fox/beamng-orbit-camera-for-assetocorsa
      extension/lua/chaser-camera/beamng-orbit/camera.lua
*/

class FBeamNGOrbitCameraMod;

struct FBeamNGOrbitCameraObjectCreateListener : public FUObjectCreateListener
{
    FBeamNGOrbitCameraMod* Owner{};
    explicit FBeamNGOrbitCameraObjectCreateListener(FBeamNGOrbitCameraMod* InOwner) : Owner(InOwner) {}
    void NotifyUObjectCreated(const UObjectBase* Object, int32 Index) override;
    void OnUObjectArrayShutdown() override;
};

struct FBeamNGOrbitCameraObjectDeleteListener : public FUObjectDeleteListener
{
    FBeamNGOrbitCameraMod* Owner{};
    explicit FBeamNGOrbitCameraObjectDeleteListener(FBeamNGOrbitCameraMod* InOwner) : Owner(InOwner) {}
    void NotifyUObjectDeleted(const UObjectBase* Object, int32 Index) override;
    void OnUObjectArrayShutdown() override;
};

class FBeamNGOrbitCameraMod : public CppUserModBase
{
public:
    FBeamNGOrbitCameraMod();

    ~FBeamNGOrbitCameraMod() override;

    void on_unreal_init() override;

    void OnObjectCreated(UObject* Object);

    void OnObjectDeleted(UObject* Object);

    void OnObjectArrayShutdown();

private:
    using FVector = MiniMath::FVector;
    using FRotator = MiniMath::FRotator;
    using FMatrix3 = MiniMath::FMatrix3;

    static constexpr double CameraDistanceMinCm = 300.0;
    static constexpr double CameraDistanceMaxCm = 3000.0;
    static constexpr double CameraPitchMinRad = -85.0 * MiniMath::DegToRad;
    static constexpr double CameraPitchMaxRad = 85.0 * MiniMath::DegToRad;

    static constexpr double DynamicFovSpeed = 130.0;
    static constexpr double DynamicPitchLowerSpeed = 1.0;
    static constexpr double DynamicPitchUpperSpeed = 10.0;
    static constexpr double DynamicPitchManualDelay = 1.0;
    static constexpr double DynamicPitchLowSpeedDelay = 1.5;
    static constexpr double DynamicPitchRiseRate = 0.30;
    static constexpr double DynamicPitchFallRate = 0.50;

    static constexpr double ManualYawLockThresholdRad = 10.0 * MiniMath::DegToRad;
    static constexpr double OrbitYawSpeedRad = 100.0 * MiniMath::DegToRad;
    static constexpr double OrbitPitchSpeedRad = 50.0 * MiniMath::DegToRad;

    // ACR adaptation: Actor::GetVelocity can be zero for custom vehicle physics.
    // We cross-check it against same-phase positional Speed, then smooth ONLY
    // the scalar Speed used by dynamic FOV/height/pitch.
    static constexpr double DynamicSpeedHalfLife = 0.12;
    static constexpr double TargetHeightAdjustCmPerSec = 50.0;

    // Used only if the persistent body mesh cannot be resolved.
    static constexpr double VehicleHalfLengthFallbackCm = 220.0;
    static constexpr double VehicleHalfHeightFallbackCm = 75.0;
    static constexpr double CollisionNearClipHalfWidthCm = 20.0;
    static constexpr double CollisionNearClipHalfHeightCm = 10.0;
    static constexpr double CollisionMinHitDistanceCm = 50.0;
    static constexpr double CollisionReleaseRate = 7.0;


    void EngineTickPre(float DeltaSeconds);

    void PollHotkeys();

    void ProcessEventPost(UObject* Context, UFunction* Function, void* Params);

    bool IsThirdFarCameraActive();

    bool UpdateOrbitCamera(UObject* Car, const FVector& CarLocation, const FRotator& CarRotation, double DeltaTimeSeconds);

    void InvalidateCollisionTraceCache();

    bool InitializeCollisionTraceCache();

    void ResetCollisionState();

    bool TraceCameraChannel(UObject* WorldContextObject, const FVector& Start, const FVector& End, double& OutHitDistanceCm);

    bool IsObstacleInFrontOfCamera(UObject* WorldContextObject, const FVector (&RayDestinations)[4]);

    FVector ApplyCameraCollision(UObject* WorldContextObject, const FVector& TargetPos, const FVector& DesiredCameraPosition,
        const FVector& DesiredCameraDirection, double DeltaTimeSeconds);

    void InitializeHeading(const FVector& TargetPos, const FVector& CarHeading);

    void HandleLockedCameraHemisphere(const FVector& TargetPos);

    void UpdateMovementHeading(const FVector& TargetPos, const FVector& CarHeading, double DeltaTimeSeconds);

    void RequestRecenter(bool bKeepPitchAndDistance, const FVector& TargetPos, const FVector& CarHeading);

    void UpdateDynamicPitchState(double Speed, bool bManualRotationActive, double DeltaTimeSeconds);

    double CalculateDynamicPitchLimit(const FVector& TargetPos, const FMatrix3& CarMatrix, const FVector& RearBottomPoint, double BaseFovDeg) const;

    void CalculateDynamicFovAndDistance(double TargetDistanceCm, double SmoothedDistanceCm, double Speed,
        const FVector& TargetPos, const FVector& RearReferencePoint, double& OutFov, double& OutDistanceCm) const;

    double CalculateDynamicHeightCm(double Speed) const;

    FOrbitInput ReadOrbitInput(double DeltaTimeSeconds);

    // ---------------- XInput gamepad ----------------
    using XInputGetStateFn = DWORD (WINAPI*)(DWORD, XINPUT_STATE*);

    void InitializeXInput();

    bool PollXInputState(XINPUT_STATE& OutState);

    void MergeGamepadInput(FOrbitInput& Out, double DeltaTimeSeconds);

    static double ClampDistanceCm(double Value);

    void InvalidateBodyGeometryCache();

    bool CacheBodyComponent(UObject* Car);

    bool ResolveBodyGeometry(UObject* Car, FBodyGeometry& OutGeometry);

#if BEAMNG_ORBIT_CAMERA_DIAGNOSTICS
    void DumpMeshAssetBounds(UObject* Asset);
#endif

#if BEAMNG_ORBIT_CAMERA_DIAGNOSTICS
    void DumpCameraManager();

    void DumpVehicleComponents(UObject* Car);
#endif

    void ResetCameraState();

    void LogFailureOnce(const CharType* Message);

private:
    FBeamNGOrbitCameraSettings Settings{};
    std::filesystem::path ConfigPath{};

    FBeamNGOrbitCameraObjectCreateListener CreateListener;
    FBeamNGOrbitCameraObjectDeleteListener DeleteListener;
    bool bCreateListenerRegistered{false};
    bool bDeleteListenerRegistered{false};

    UObject* BodyOwner{nullptr};
    UObject* BodyComponent{nullptr};
    UObject* BodyMeshAsset{nullptr};
    FVector BodyLocalOrigin{};
    FVector BodyLocalExtent{};
    bool bBodyBoundsValid{false};

    UObject* CameraManager{nullptr};
    UObject* KismetSystemLibrary{nullptr};
    UFunction* LineTraceSingleFunction{nullptr};

    bool bCollisionTraceCacheValid{false};
    FProperty* CollisionWorldContextProperty{nullptr};
    FProperty* CollisionTraceChannelProperty{nullptr};
    FProperty* CollisionActorsToIgnoreProperty{nullptr};
    FProperty* CollisionReturnProperty{nullptr};

    FStructProperty* CollisionStartProperty{nullptr};
    FStructProperty* CollisionEndProperty{nullptr};
    FStructProperty* CollisionOutHitProperty{nullptr};

    FProperty* CollisionStartXProperty{nullptr};
    FProperty* CollisionStartYProperty{nullptr};
    FProperty* CollisionStartZProperty{nullptr};

    FProperty* CollisionEndXProperty{nullptr};
    FProperty* CollisionEndYProperty{nullptr};
    FProperty* CollisionEndZProperty{nullptr};

    FProperty* CollisionHitDistanceProperty{nullptr};

    std::vector<uint8_t> CollisionTraceParams{};

    bool bDeleteWasDown{false};
    bool bInsertWasDown{false};
#if BEAMNG_ORBIT_CAMERA_DIAGNOSTICS
    bool bEndWasDown{false};
    bool bCtrlBWasDown{false};
    std::atomic_bool bCameraManagerDumpRequested{false};
    std::atomic_bool bComponentDumpRequested{false};
#endif

    bool bNum5WasDown{false};

    HMODULE XInputModule{nullptr};
    XInputGetStateFn XInputGetState{nullptr};
    int GamepadUserIndex{-1};
    bool bGamepadConnected{false};
    bool bGamepadR3WasDown{false};
    bool bGamepadL3WasDown{false};

    std::atomic_bool bEnabled{false};
    std::atomic_bool bCollisionEnabled{true};
    std::atomic<uint64_t> FrameSerial{0};
    std::atomic<double> DeltaTime{1.0/60.0};

    bool bWasThirdFarCameraActive{false};

    bool bOrbitInitialized{false};
    double OrbitYawRad{0.0};
    double OrbitPitchRad{0.0};
    double DisplayedYawRad{0.0};
    double DisplayedPitchRad{0.0};
    double OrbitDistanceCm{0.0};
    double DisplayedDistanceCm{0.0};
    bool bLockCamera{false};
    double AccumulatedManualYawRad{0.0};
    double LastAppliedFov{65.0};

    FVector HeadingReference{0.0,1.0,0.0};
    FVector LastValidCarHeading{0.0,1.0,0.0};
    FVector CameraAnchor{};
    FVector CameraAnchorPerpendicular{};
    FVector LastTargetPosition{};
    bool bHeadingInitialized{false};
    bool bResetHeadingReference{false};

    double TimeSinceManualRotation{1000.0};
    bool bAbovePitchSpeedThreshold{false};
    double BelowPitchThresholdTimer{-1.0};
    double DynamicPitchBlend{0.0};
    double DynamicPitchVelocity{0.0};

    FVector PreviousCarPosition{};
    bool bPreviousCarPositionValid{false};

    double DynamicSpeedMps{0.0};
    bool bDynamicSpeedInitialized{false};
    double TargetHeightOffsetCm{0.0};
#if BEAMNG_ORBIT_CAMERA_DIAGNOSTICS
    double DiagnosticTime{0.0};
#endif

    bool bCollisionUseRaycast{true};
    bool bCollisionHasLastNearClipCenter{false};
    bool bCollisionDistanceValid{false};
    bool bCollisionHitThisFrame{false};
    bool bLoggedCollisionTraceUnavailable{false};
    int32_t CollisionTraceCountThisFrame{0};
    FVector CollisionLastNearClipCenter{};
    double CollisionLastDistanceCm{0.0};
    double CollisionDesiredDistanceCm{0.0};
    double CollisionAppliedDistanceCm{0.0};

    bool bOutputValid{false};
    uint64_t LastComputedFrame{std::numeric_limits<uint64_t>::max()};
    FVector OutputLocation{};
    FRotator OutputRotation{};
    double OutputFov{65.0};

    bool bLoggedFirstOutput{false};
    bool bLoggedFailure{false};
    bool bLoggedGetCurrentCameraUnavailable{false};
};
