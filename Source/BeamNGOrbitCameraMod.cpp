#include "BeamNGOrbitCameraMod.hpp"

#include "Core/MiniMath.hpp"
#include "Core/UnrealReflection.hpp"

#include <Unreal/FAssetData.hpp>
#include <Unreal/UAssetRegistryHelpers.hpp>

#include <algorithm>
#include <cmath>

namespace MiniMath = BeamNGOrbitCamera::MiniMath;
using namespace BeamNGOrbitCamera::MiniMath;
using namespace BeamNGOrbitCamera::Reflection;

namespace
{
    constexpr auto DedicatedModifierClassName = STR("BP_BeamNGOrbitModifier_C");
    constexpr auto DedicatedModifierPackageName =
        STR("/Game/Mods/BeamNGOrbitModifier/BP_BeamNGOrbitModifier");
    constexpr auto DedicatedModifierClassFullName =
        STR("BlueprintGeneratedClass /Game/Mods/BeamNGOrbitModifier/BP_BeamNGOrbitModifier.BP_BeamNGOrbitModifier_C");
}

FBeamNGOrbitCameraMod::FBeamNGOrbitCameraMod() : CppUserModBase(), CreateListener(this), DeleteListener(this)
{
    ModName = STR("BeamNGOrbitCamera");
    ModVersion = STR("0.13.4");
    ModDescription = STR("BeamNG-style orbit camera core for Assetto Corsa Rally");
    ModAuthors = STR("Brown2Fox");

    ConfigPath = FBeamNGOrbitCameraConfig::ResolveConfigPath();

    const bool bConfigReady = FBeamNGOrbitCameraConfig::Load(Settings, ConfigPath);

    bCollisionEnabled.store(Settings.bCollisionEnabled, std::memory_order_release);

    TargetHeightOffsetCm = Settings.TargetHeightOffsetCm;

    if (!bConfigReady)
    {
        Output::send<LogLevel::Warning>(STR("[BeamNGOrbitCamera] Config could not be loaded/saved: {}\n"), ConfigPath.wstring());
    }
}

FBeamNGOrbitCameraMod::~FBeamNGOrbitCameraMod()
{
    bEnabled.store(false, std::memory_order_release);
    if (bCreateListenerRegistered)
    {
        UObjectArray::RemoveUObjectCreateListener(&CreateListener);
        bCreateListenerRegistered = false;
    }
    if (bDeleteListenerRegistered)
    {
        UObjectArray::RemoveUObjectDeleteListener(&DeleteListener);
        bDeleteListenerRegistered = false;
    }

    CreateListener.Owner = nullptr;
    DeleteListener.Owner = nullptr;
    CameraManager = nullptr;
    DedicatedModifierClass = nullptr;
    DedicatedModifierInstance = nullptr;
    InvalidateCollisionTraceCache();
    InvalidateBodyGeometryCache();

    if (XInputModule)
    {
        FreeLibrary(XInputModule);
        XInputModule = nullptr;
        XInputGetState = nullptr;
    }
}

void FBeamNGOrbitCameraMod::on_unreal_init()
{
    Output::send<LogLevel::Verbose>(
        STR("[BeamNGOrbitCamera] v0.13.4 experimental | tested with ACR Steam build 24097451 | UE4SS 1c1a1497 | target ThirdFar | self-loaded modifier\n")
    );

    UObjectArray::AddUObjectCreateListener(&CreateListener);
    UObjectArray::AddUObjectDeleteListener(&DeleteListener);
    bCreateListenerRegistered = true;
    bDeleteListenerRegistered = true;

    InitializeXInput();

    KismetSystemLibrary = UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, STR("/Script/Engine.Default__KismetSystemLibrary"));

    if (KismetSystemLibrary)
    {
        LineTraceSingleFunction = KismetSystemLibrary->GetFunctionByNameInChain(FromCharTypePtr<TCHAR>(STR("LineTraceSingle")));
    }

    InitializeCollisionTraceCache();

    Hook::RegisterEngineTickPreCallback([this](auto&, UEngine*, float DeltaSeconds, bool) {
            EngineTickPre(DeltaSeconds);
        }, {false, false, STR("BeamNGOrbitCamera"), STR("FrameClock")});

    Hook::RegisterProcessEventPostCallback([this](auto&, UObject* Context, UFunction* Function, void* Params) {
            ProcessEventPost(Context, Function, Params);
        }, {false, false, STR("BeamNGOrbitCamera"), STR("OrbitCamera")});

#if BEAMNG_ORBIT_CAMERA_DIAGNOSTICS
    Output::send<LogLevel::Verbose>(
        STR("[BeamNGOrbitCamera] Hotkeys: Camera toggle [Delete] | Collision toggle [Insert] | Manager dump [End] | Component dump [Ctrl+B] | ")
        STR("Orbit [Right Stick] | Zoom [RB/R1 + Left Stick Y] | Recenter [R3/L3] | NumPad controls\n")
    );
#else
    Output::send<LogLevel::Verbose>(
        STR("[BeamNGOrbitCamera] Hotkeys: Camera toggle [Delete] | Collision toggle [Insert] | Orbit [Right Stick] | ")
        STR("Zoom [RB/R1 + Left Stick Y] | Recenter [R3/L3] | NumPad controls\n")
    );
#endif

    if (!bCollisionTraceCacheValid)
    {
        Output::send<LogLevel::Warning>(STR("[BeamNGOrbitCamera] Collision trace cache is unavailable.\n"));
    }
}

void FBeamNGOrbitCameraMod::OnObjectCreated(UObject* Object)
{
    if (!Object) return;
    auto* Class = Object->GetClassPrivate();
    if (!Class || Class->GetName() != STR("BC_CarPlayerCameraManager_C")) return;
    if (Object->HasAnyFlags(RF_ClassDefaultObject)) return;

    const auto FullName = Object->GetFullName();
    if (FullName.contains(STR("MainMenu"))) return;

    CameraManager = Object;
    DedicatedModifierInstance = nullptr;
    bLoggedDedicatedModifierInstallFailure = false;
    bEnabled.store(false, std::memory_order_release);
    ResetCameraState();
}

void FBeamNGOrbitCameraMod::OnObjectDeleted(UObject* Object)
{
    if (Object == KismetSystemLibrary || Object == LineTraceSingleFunction)
    {
        InvalidateCollisionTraceCache();
    }

    if (Object && (Object == BodyComponent || Object == BodyMeshAsset || Object == BodyOwner))
    {
        InvalidateBodyGeometryCache();
    }

    if (Object && Object == CameraManager)
    {
        CameraManager = nullptr;
        DedicatedModifierInstance = nullptr;
        bEnabled.store(false, std::memory_order_release);
        ResetCameraState();
    }

    if (Object && Object == DedicatedModifierInstance)
    {
        DedicatedModifierInstance = nullptr;
    }

    if (Object && Object == DedicatedModifierClass)
    {
        DedicatedModifierClass = nullptr;
        DedicatedModifierInstance = nullptr;
    }
}

void FBeamNGOrbitCameraMod::OnObjectArrayShutdown()
{
    bCreateListenerRegistered = false;
    bDeleteListenerRegistered = false;
    CameraManager = nullptr;
    DedicatedModifierClass = nullptr;
    DedicatedModifierInstance = nullptr;
    InvalidateCollisionTraceCache();
    InvalidateBodyGeometryCache();
    bEnabled.store(false, std::memory_order_release);
    ResetCameraState();
}

void FBeamNGOrbitCameraMod::EngineTickPre(float DeltaSeconds)
{
    // The previous frame's BlueprintModifyCamera output is stored in
    // CameraCachePrivate and becomes input to the next camera update. Restore
    // the game's clean output before that update so the orbit override remains
    // a presentation-only result instead of feeding back into camera blending
    // and modifier state.
    RestoreCleanCameraPose();
    EnsureDedicatedCameraModifier();

    const double DeltaTimeSeconds = std::clamp(static_cast<double>(DeltaSeconds), 1.0 / 500.0, 0.05);

    DeltaTime.store(DeltaTimeSeconds, std::memory_order_release);
    FrameSerial.fetch_add(1, std::memory_order_acq_rel);

    PollHotkeys();
}

void FBeamNGOrbitCameraMod::EnsureDedicatedCameraModifier()
{
    if (!CameraManager || DedicatedModifierInstance)
        return;

    if (!DedicatedModifierClass)
    {
        // Assets from a separately mounted mod container are not necessarily
        // merged into the game's searchable Asset Registry. UE4SS's
        // BPModLoader handles UE 5.1+ packages the same way: build the minimal
        // FAssetData key and ask AssetRegistryHelpers to load it directly.
        FAssetData ModifierAssetData{};
        ModifierAssetData.SetPackageName(FName(DedicatedModifierPackageName, FNAME_Add));
        ModifierAssetData.SetAssetName(FName(DedicatedModifierClassName, FNAME_Add));

        UObject* LoadedClassObject = UAssetRegistryHelpers::GetAsset(ModifierAssetData);
        if (LoadedClassObject)
        {
            DedicatedModifierClass = static_cast<UClass*>(LoadedClassObject);
        }

        if (!DedicatedModifierClass)
        {
            if (!bLoggedDedicatedModifierInstallFailure)
            {
                bLoggedDedicatedModifierInstallFailure = true;
                Output::send<LogLevel::Warning>(
                    STR("[BeamNGOrbitCamera] Could not load dedicated camera modifier class: {} / {}.\n"),
                    DedicatedModifierPackageName,
                    DedicatedModifierClassName
                );
            }
            return;
        }
    }

    UObject* Modifier = nullptr;
    if (!CallClassArgObjectFunction(CameraManager, STR("FindCameraModifierByClass"),
            STR("ModifierClass"), DedicatedModifierClass, Modifier))
    {
        if (!bLoggedDedicatedModifierInstallFailure)
        {
            bLoggedDedicatedModifierInstallFailure = true;
            Output::send<LogLevel::Warning>(
                STR("[BeamNGOrbitCamera] FindCameraModifierByClass reflection failed.\n")
            );
        }
        return;
    }

    const bool bAlreadyInstalled = Modifier != nullptr;
    if (!Modifier && !CallClassArgObjectFunction(CameraManager, STR("AddNewCameraModifier"),
            STR("ModifierClass"), DedicatedModifierClass, Modifier))
    {
        if (!bLoggedDedicatedModifierInstallFailure)
        {
            bLoggedDedicatedModifierInstallFailure = true;
            Output::send<LogLevel::Warning>(
                STR("[BeamNGOrbitCamera] AddNewCameraModifier reflection failed.\n")
            );
        }
        return;
    }

    if (!Modifier)
    {
        if (!bLoggedDedicatedModifierInstallFailure)
        {
            bLoggedDedicatedModifierInstallFailure = true;
            Output::send<LogLevel::Warning>(
                STR("[BeamNGOrbitCamera] AddNewCameraModifier returned null.\n")
            );
        }
        return;
    }

    DedicatedModifierInstance = Modifier;
    bLoggedDedicatedModifierInstallFailure = false;

    if (!bLoggedDedicatedModifierInstalled)
    {
        bLoggedDedicatedModifierInstalled = true;
        Output::send<LogLevel::Verbose>(
            STR("[BeamNGOrbitCamera] Dedicated camera modifier installed on vehicle camera manager ({}).\n"),
            bAlreadyInstalled ? STR("existing") : STR("created")
        );
    }
}

void FBeamNGOrbitCameraMod::RestoreCleanCameraPose()
{
    if (!bCleanPosePending)
        return;

    UObject* Manager = PendingCleanPoseManager;
    const FCameraPose CleanPose = PendingCleanPose;
    const FCameraPose WrittenPose = PendingWrittenPose;

    bCleanPosePending = false;
    PendingCleanPoseManager = nullptr;

    if (!Manager || Manager != CameraManager)
        return;

    FCameraPose LivePose{};
    if (!ReadCameraCachePose(Manager, LivePose))
    {
        if (!bLoggedCleanPoseRestoreUnavailable)
        {
            bLoggedCleanPoseRestoreUnavailable = true;
            Output::send<LogLevel::Warning>(
                STR("[BeamNGOrbitCamera] CameraCachePrivate.POV is unavailable; clean-camera isolation is disabled for this frame.\n")
            );
        }
        return;
    }

    // A cut, camera switch, later modifier, or another mod may have replaced
    // the cache after our callback. Only take our own exact pose back out.
    if (!CameraPosesMatch(LivePose, WrittenPose))
        return;

    if (!WriteCameraCachePose(Manager, CleanPose))
    {
        if (!bLoggedCleanPoseRestoreUnavailable)
        {
            bLoggedCleanPoseRestoreUnavailable = true;
            Output::send<LogLevel::Warning>(
                STR("[BeamNGOrbitCamera] Failed to restore CameraCachePrivate.POV; clean-camera isolation is disabled for this frame.\n")
            );
        }
        return;
    }

    if (!bLoggedCleanPoseIsolationActive)
    {
        bLoggedCleanPoseIsolationActive = true;
        Output::send<LogLevel::Verbose>(
            STR("[BeamNGOrbitCamera] Clean camera-cache isolation active.\n")
        );
    }
}

bool FBeamNGOrbitCameraMod::CameraPosesMatch(const FCameraPose& Left, const FCameraPose& Right)
{
    // UE5 LWC Location/Rotation are doubles and are copied byte-for-byte from
    // the Blueprint out parameters. FOV is a float, so compare its canonical
    // stored representation rather than the double used by our math.
    return Left.Location.X == Right.Location.X
        && Left.Location.Y == Right.Location.Y
        && Left.Location.Z == Right.Location.Z
        && Left.Rotation.Pitch == Right.Rotation.Pitch
        && Left.Rotation.Yaw == Right.Rotation.Yaw
        && Left.Rotation.Roll == Right.Rotation.Roll
        && static_cast<float>(Left.Fov) == static_cast<float>(Right.Fov);
}

void FBeamNGOrbitCameraMod::ProcessEventPost(UObject* Context, UFunction* Function, void* Params)
{
    if (!CameraManager || !Context || !Function || !Params) return;
    if (Function->GetName() != STR("BlueprintModifyCamera")) return;

    auto* ContextClass = Context->GetClassPrivate();
    if (!ContextClass || ContextClass->GetName() != DedicatedModifierClassName) return;
    if (ContextClass->GetFullName() != DedicatedModifierClassFullName) return;
    if (ReadObjectPropertyByName(Context, STR("CameraOwner")) != CameraManager) return;

    DedicatedModifierInstance = Context;

    if (!bLoggedDedicatedModifierActive)
    {
        bLoggedDedicatedModifierActive = true;
        Output::send<LogLevel::Verbose>(
            STR("[BeamNGOrbitCamera] Dedicated camera modifier callback active.\n")
        );
    }

#if BEAMNG_ORBIT_CAMERA_DIAGNOSTICS
    if (bCameraManagerDumpRequested.exchange(false, std::memory_order_acq_rel))
    {
        DumpCameraManager();
    }

    if (bComponentDumpRequested.exchange(false, std::memory_order_acq_rel))
    {
        UObject* DumpCar = ResolveViewTargetActor(CameraManager);
        if (DumpCar)
            DumpVehicleComponents(DumpCar);
        else
            Output::send<LogLevel::Warning>(STR("[BeamNGOrbitCamera] COMPONENT CENSUS: ViewTarget.Target is null.\n"));
    }
#endif
    if (!bEnabled.load(std::memory_order_acquire))
    {
        bWasThirdFarCameraActive = false;
        return;
    }

    const bool bThirdFarCameraActive = IsThirdFarCameraActive();
    if (bThirdFarCameraActive != bWasThirdFarCameraActive)
    {
        ResetCameraState();
        bWasThirdFarCameraActive = bThirdFarCameraActive;
    }

    if (!bThirdFarCameraActive)
        return;

    FStructAccess location{};
    FStructAccess rotation{};
    if (!ResolveStructParam(Function, Params, STR("NewViewLocation"), location) || !ResolveStructParam(Function, Params, STR("NewViewRotation"), rotation))
    {
        LogFailureOnce(STR("failed to resolve NewViewLocation/NewViewRotation"));
        return;
    }

    FProperty* FovProperty = Function->GetPropertyByNameInChain(STR("NewFOV"));
    if (!FovProperty)
    {
        LogFailureOnce(STR("BlueprintModifyCamera.NewFOV not found"));
        return;
    }

    FCameraPose CleanPose{};
    const bool bCleanPoseValid = ReadVec3(location, CleanPose.Location)
        && ReadRot3(rotation, CleanPose.Rotation)
        && ReadScalar(FovProperty, Params, CleanPose.Fov);

    UObject* Car = ResolveViewTargetActor(CameraManager);
    if (!Car)
    {
        LogFailureOnce(STR("failed to resolve ViewTarget.Target vehicle"));
        return;
    }

    MiniMath::FVector CarLocation{};
    MiniMath::FRotator CarRotation{};
    if (!CallActorLocation(Car, CarLocation) || !CallActorRotation(Car, CarRotation))
    {
        LogFailureOnce(STR("failed to sample same-phase vehicle transform"));
        return;
    }

    const uint64_t frame = FrameSerial.load(std::memory_order_acquire);
    if (!bOutputValid || LastComputedFrame != frame)
    {
        const double DeltaTimeSeconds = DeltaTime.load(std::memory_order_acquire);
        if (!UpdateOrbitCamera(Car, CarLocation, CarRotation, DeltaTimeSeconds)) return;
        LastComputedFrame = frame;
        bOutputValid = true;
    }

    if (!WriteVec3(location, OutputLocation) || !WriteRot3(rotation, OutputRotation) || !WriteScalar(FovProperty, Params, OutputFov))
    {
        LogFailureOnce(STR("failed to write final Location/Rotation/FOV"));
        return;
    }

    if (bCleanPoseValid)
    {
        PendingCleanPoseManager = CameraManager;
        PendingCleanPose = CleanPose;
        PendingWrittenPose = {OutputLocation, OutputRotation, OutputFov};
        bCleanPosePending = true;
    }
    else
    {
        bCleanPosePending = false;
        PendingCleanPoseManager = nullptr;

        if (!bLoggedCleanPoseCaptureUnavailable)
        {
            bLoggedCleanPoseCaptureUnavailable = true;
            Output::send<LogLevel::Warning>(
                STR("[BeamNGOrbitCamera] Failed to capture the clean BlueprintModifyCamera output; clean-camera isolation is unavailable.\n")
            );
        }
    }

    if (!bLoggedFirstOutput)
    {
        bLoggedFirstOutput = true;
        Output::send<LogLevel::Verbose>(STR("[BeamNGOrbitCamera] ThirdFar override active.\n"));
    }
}

bool FBeamNGOrbitCameraMod::IsThirdFarCameraActive()
{
    UObject* CurrentCamera = nullptr;
    if (!CallNoArgObjectFunction(CameraManager, STR("GetCurrentCamera"), CurrentCamera))
    {
        if (!bLoggedGetCurrentCameraUnavailable)
        {
            bLoggedGetCurrentCameraUnavailable = true;
            Output::send<LogLevel::Warning>(
                STR("[BeamNGOrbitCamera] GetCurrentCamera is unavailable; the game or UE4SS version may be incompatible.\n")
            );
        }
        return false;
    }

    return CurrentCamera && CurrentCamera->GetName() == STR("ThirdFar");
}

bool FBeamNGOrbitCameraMod::UpdateOrbitCamera(
    UObject* Car, const MiniMath::FVector& CarLocation, const MiniMath::FRotator& CarRotation, double DeltaTimeSeconds)
{
    const MiniMath::FMatrix3 CarMatrix = RotatorToMatrix(CarRotation);
    MiniMath::FVector CarHeading = PlanarHeading(MatrixVehicleForward(CarMatrix));
    if (Length(CarHeading) < 0.0001)
        CarHeading = bHeadingInitialized ? HeadingReference : LastValidCarHeading;
    else
        LastValidCarHeading = CarHeading;

    FBodyGeometry BodyGeometry{};
    const bool bBodyGeometryValid = ResolveBodyGeometry(Car, BodyGeometry);

    // Use the permanent body mesh center as the vehicle-relative camera target.
    // Unlike an Actor-origin + world-up offset, this point follows the body
    // correctly through rollovers. Keep the calibrated actor-origin fallback
    // for cars whose body mesh cannot be resolved.
    const MiniMath::FVector TargetReference = bBodyGeometryValid
            ? BodyGeometry.Center : CarLocation + WorldUp() * 60.0;

    const MiniMath::FVector TargetUp = bBodyGeometryValid
            ? BodyGeometry.Up : WorldUp();

    const FOrbitInput Input = ReadOrbitInput(DeltaTimeSeconds);
    TargetHeightOffsetCm = std::clamp(TargetHeightOffsetCm + Input.TargetHeightStepCm, -100.0, 200.0);

    const MiniMath::FVector TargetPos = TargetReference + TargetUp * TargetHeightOffsetCm;

    // Actor::GetVelocity is only guaranteed To reflect root-Component physics
    // or a MovementComponent. ACR can use custom vehicle physics, so also
    // measure same-phase positional Speed and fall back To it if Actor velocity
    // is effectively zero while the Car is clearly moving.
    double ActorSpeedMps = 0.0;
    MiniMath::FVector CarVelocityCmps{};
    const bool bActorVelocityValid = CallActorVelocity(Car, CarVelocityCmps);
    if (bActorVelocityValid)
        ActorSpeedMps = Length(CarVelocityCmps) / 100.0;

    double PositionSpeedMps = 0.0;
    if (bPreviousCarPositionValid && DeltaTimeSeconds > 0.0001)
    {
        const double DeltaCm = Length(CarLocation - PreviousCarPosition);
        if (DeltaCm < 2500.0)
            PositionSpeedMps = (DeltaCm / 100.0) / DeltaTimeSeconds;
    }

    PreviousCarPosition = CarLocation;
    bPreviousCarPositionValid = true;

    double RawSpeedMps = ActorSpeedMps;
    if (!bActorVelocityValid || (ActorSpeedMps < 0.25 && PositionSpeedMps > 1.0))
    {
        RawSpeedMps = PositionSpeedMps;
    }

    RawSpeedMps = std::clamp(RawSpeedMps, 0.0, 150.0);

    if (!bDynamicSpeedInitialized)
    {
        DynamicSpeedMps = RawSpeedMps;
        bDynamicSpeedInitialized = true;
    }
    else
    {
        const double SpeedAlpha = 1.0 - std::exp(-0.6931471805599453 * DeltaTimeSeconds / DynamicSpeedHalfLife);
        DynamicSpeedMps += (RawSpeedMps - DynamicSpeedMps) * SpeedAlpha;
    }

    const double SpeedMps = DynamicSpeedMps;

    if (!bOrbitInitialized)
    {
        OrbitPitchRad = Settings.CameraPitchRad;
        DisplayedPitchRad = OrbitPitchRad;
        OrbitYawRad = 0.0;
        DisplayedYawRad = 0.0;
        OrbitDistanceCm = Settings.CameraDistanceCm;
        DisplayedDistanceCm = OrbitDistanceCm;
        LastAppliedFov = Settings.CameraFovDeg;
        InitializeHeading(TargetPos, CarHeading);
        bOrbitInitialized = true;
    }

    HandleLockedCameraHemisphere(TargetPos);
    UpdateMovementHeading(TargetPos, CarHeading, DeltaTimeSeconds);

    if (Input.bRecenter || Input.bRecenterKeep)
        RequestRecenter(Input.bRecenterKeep, TargetPos, CarHeading);

    const bool bManualRotationActive = std::abs(Input.YawStepRad) > 0.0001 || std::abs(Input.PitchStepRad) > 0.0001;

    OrbitYawRad = WrapRadians(OrbitYawRad + Input.YawStepRad);
    OrbitPitchRad = std::clamp(OrbitPitchRad + Input.PitchStepRad, CameraPitchMinRad, CameraPitchMaxRad);

    if (std::abs(Input.YawStepRad) > 0.0001)
        AccumulatedManualYawRad += Input.YawStepRad;
    if (std::abs(AccumulatedManualYawRad) > ManualYawLockThresholdRad)
        bLockCamera = true;

    // Source camera.lua rendered rotation smoothing.
    const double RotationT = (DeltaTimeSeconds * 8.0) / (1.0 + DeltaTimeSeconds * 8.0);
    const double MaxYawSpeed = bManualRotationActive ? 1000.0 : 4.5;
    const double YawError = ShortestAngleDifference(DisplayedYawRad, OrbitYawRad);
    const double RenderedYawStep = std::clamp(YawError * RotationT, -MaxYawSpeed * DeltaTimeSeconds, MaxYawSpeed * DeltaTimeSeconds);
    DisplayedYawRad = WrapRadians(DisplayedYawRad + RenderedYawStep);
    DisplayedPitchRad = std::clamp(DisplayedPitchRad + (OrbitPitchRad - DisplayedPitchRad) * RotationT, CameraPitchMinRad, CameraPitchMaxRad);

    // Source keyboard zoom formula, metres converted To cm.
    OrbitDistanceCm = ClampDistanceCm(OrbitDistanceCm + Input.ZoomStep * 0.1 * LastAppliedFov * 100.0);
    const double DistanceT = (DeltaTimeSeconds * 8.0) / (1.0 + DeltaTimeSeconds * 8.0);
    DisplayedDistanceCm += (OrbitDistanceCm - DisplayedDistanceCm) * DistanceT;

    UpdateDynamicPitchState(SpeedMps, bManualRotationActive, DeltaTimeSeconds);

    const double VehicleHalfLengthCm = bBodyGeometryValid
            ? BodyGeometry.Extent.Y : VehicleHalfLengthFallbackCm;

    const double VehicleHalfHeightCm = bBodyGeometryValid
            ? BodyGeometry.Extent.Z : VehicleHalfHeightFallbackCm;

    const MiniMath::FVector RearReferencePoint = bBodyGeometryValid
            ? BodyGeometry.Center
                - BodyGeometry.Forward * VehicleHalfLengthCm : TargetReference
                - CarHeading * VehicleHalfLengthCm;

    const MiniMath::FVector RearBottomPoint = bBodyGeometryValid
            ? RearReferencePoint
                - BodyGeometry.Up * VehicleHalfHeightCm : RearReferencePoint
                - WorldUp() * VehicleHalfHeightCm;

    const MiniMath::FVector OrbitForward = Normalized(RotateAroundAxis(HeadingReference, WorldUp(), DisplayedYawRad), CarHeading);

    double DynamicFov = Settings.CameraFovDeg;
    double DynamicDistanceCm = DisplayedDistanceCm;
    CalculateDynamicFovAndDistance(OrbitDistanceCm, DisplayedDistanceCm, SpeedMps, TargetPos, RearReferencePoint, DynamicFov, DynamicDistanceCm);

    const double HorizontalDistance = std::cos(DisplayedPitchRad) * DynamicDistanceCm;
    const double VerticalDistance = std::sin(DisplayedPitchRad) * DynamicDistanceCm;

    const MiniMath::FVector BaseCameraPosition = TargetPos - OrbitForward * HorizontalDistance + WorldUp() * VerticalDistance;

    const double DynamicHeightCm = CalculateDynamicHeightCm(SpeedMps);

    const MiniMath::FVector FinalCameraPosition = BaseCameraPosition + WorldUp() * DynamicHeightCm;

    MiniMath::FVector BaseDirection = Normalized(TargetPos - BaseCameraPosition, OrbitForward);
    double DynamicPitchAngleRad = 0.0;
    double DynamicPitchLimitRad = 0.0;
    if (DynamicPitchBlend > 0.0001 && Settings.DynamicPitchAtSpeedRad > 0.0)
    {
        DynamicPitchLimitRad = CalculateDynamicPitchLimit(TargetPos, CarMatrix, RearBottomPoint, Settings.CameraFovDeg);
        DynamicPitchAngleRad = -std::min(Settings.DynamicPitchAtSpeedRad, DynamicPitchLimitRad)
            * DynamicPitchBlend;
    }

    MiniMath::FVector FinalDirection = BaseDirection;
    MiniMath::FVector CameraRight = Cross(WorldUp(), BaseDirection);
    if (Length(CameraRight) > 0.0001 && std::abs(DynamicPitchAngleRad) > 0.000001)
    {
        CameraRight = Normalized(CameraRight, MiniMath::FVector{0.0, 1.0, 0.0});
        FinalDirection = Normalized(RotateAroundAxis(BaseDirection, CameraRight, DynamicPitchAngleRad), BaseDirection);
    }

    MiniMath::FVector CollisionCorrectedPosition = FinalCameraPosition;

    if (bCollisionEnabled.load(std::memory_order_acquire))
    {
        CollisionCorrectedPosition = ApplyCameraCollision(Car, TargetPos, FinalCameraPosition, FinalDirection, DeltaTimeSeconds);
    }
    else
    {
        ResetCollisionState();
    }

    OutputLocation = CollisionCorrectedPosition;
    OutputRotation = DirectionToRotator(FinalDirection);

    // CSP/Assetto Corsa uses VERTICAL FOV. Unreal FMinimalViewInfo::FOV
    // is HORIZONTAL FOV. Keep all BeamNG/AC camera math, including dolly
    // compensation, in the original vertical-FOV space and convert only
    // the final Value sent To Unreal.
    const double AspectRatio = ReadViewTargetAspectRatio(CameraManager);
    const double UnrealHorizontalFov = VerticalToHorizontalFovDeg(DynamicFov, AspectRatio);

    OutputFov = std::clamp(UnrealHorizontalFov, 20.0, 160.0);

    // Keep source-style zoom/dolly calculations in AC vertical-FOV units.
    LastAppliedFov = DynamicFov;

#if BEAMNG_ORBIT_CAMERA_DIAGNOSTICS
    DiagnosticTime += DeltaTimeSeconds;
    if (DiagnosticTime >= 1.0)
    {
        DiagnosticTime = 0.0;

        const bool bCollisionOn = bCollisionEnabled.load(std::memory_order_acquire);
        const CharType* CollisionMode = bCollisionOn ? (bCollisionUseRaycast ? STR("FULL") : STR("EDGE")) : STR("OFF");

        Output::send<LogLevel::Verbose>(
            STR("[BeamNGOrbitCamera] DYN speed={:.1f}m/s | body={} | col={} mode={} rays={} hit={} desired={:.2f}m applied={:.2f}m\n"),
            SpeedMps, bBodyGeometryValid ? STR("YES") : STR("NO"), bCollisionOn ? STR("ON") : STR("OFF"), CollisionMode, CollisionTraceCountThisFrame,
            bCollisionHitThisFrame ? STR("YES") : STR("NO"), CollisionDesiredDistanceCm / 100.0, CollisionAppliedDistanceCm / 100.0);
    }
#endif

    return true;
}

double FBeamNGOrbitCameraMod::ClampDistanceCm(double Value)
{
    return std::clamp(Value, CameraDistanceMinCm, CameraDistanceMaxCm);
}

void FBeamNGOrbitCameraMod::InitializeHeading(const MiniMath::FVector& TargetPos, const MiniMath::FVector& CarHeading)
{
    HeadingReference = CarHeading;
    CameraAnchor = TargetPos - CarHeading * Settings.CameraRelaxationCm;

    MiniMath::FVector Right = Cross(WorldUp(), CarHeading);
    const double RightLen = Length(Right);
    CameraAnchorPerpendicular = RightLen > 0.0001
        ? TargetPos + Right * (-Settings.CameraRelaxationCm * 0.8 / RightLen) : TargetPos;

    LastTargetPosition = TargetPos;
    bHeadingInitialized = true;
    bResetHeadingReference = false;
}

void FBeamNGOrbitCameraMod::HandleLockedCameraHemisphere(const MiniMath::FVector& TargetPos)
{
    if (!bLockCamera || !bHeadingInitialized) return;

    const MiniMath::FVector LockedDirection = LastTargetPosition - CameraAnchor;
    if (Length(LockedDirection) < 0.0001) return;

    const MiniMath::FVector MoveDirection = TargetPos - LastTargetPosition;
    if (Dot(MoveDirection, LockedDirection) >= 0.0) return;

    OrbitYawRad = WrapRadians(OrbitYawRad + Pi);
    DisplayedYawRad = WrapRadians(DisplayedYawRad + Pi);

    // Original port flips Y because AC is Y-up. ACR/Unreal is Z-up.
    const MiniMath::FVector FlipOffset{LockedDirection.X, LockedDirection.Y, -LockedDirection.Z};
    CameraAnchor = TargetPos + FlipOffset;

    MiniMath::FVector Right = Cross(WorldUp(), LockedDirection);
    const double RightLen = Length(Right);
    CameraAnchorPerpendicular = RightLen > 0.0001
        ? TargetPos + Right * (-Settings.CameraRelaxationCm * 0.8 / RightLen) : TargetPos;
}

void FBeamNGOrbitCameraMod::UpdateMovementHeading(const MiniMath::FVector& TargetPos, const MiniMath::FVector& CarHeading, double DeltaTimeSeconds)
{
    if (!bHeadingInitialized || bResetHeadingReference || DeltaTimeSeconds > 0.2 || Length(TargetPos - LastTargetPosition) > 2500.0)
    {
        InitializeHeading(TargetPos, CarHeading);
        return;
    }

    MiniMath::FVector PointVec = TargetPos - CameraAnchor;
    MiniMath::FVector PerpVec = CameraAnchorPerpendicular - TargetPos;
    double PointLen = Length(PointVec);
    const double PerpLen = Length(PerpVec);

    if (PointLen < Settings.CameraRelaxationCm && PerpLen > Settings.CameraRelaxationCm * 0.8)
    {
        MiniMath::FVector Move = TargetPos - LastTargetPosition;
        const double MoveLen = Length(Move);
        if (MoveLen > 0.0001 && PointLen > 0.0001 && PerpLen > 0.0001)
        {
            Move = Move / MoveLen;
            const double PointAlong = std::abs(Dot(PointVec / PointLen, Move));
            const double PerpAlong = std::abs(Dot(PerpVec / PerpLen, Move));
            if (PointAlong > PerpAlong)
            {
                const MiniMath::FVector CrossA = Cross(PointVec, PerpVec);
                const MiniMath::FVector CrossB = Cross(CrossA, PerpVec);
                const double CorrectedLen = Length(CrossB);
                if (CorrectedLen > 0.0001)
                {
                    CameraAnchor = TargetPos + CrossB / CorrectedLen;
                    PointVec = TargetPos - CameraAnchor;
                    PointLen = Length(PointVec);
                }
            }
        }
    }

    HeadingReference = PointLen > 0.0001 ? PointVec / PointLen : CarHeading;

    // Z-up adaptation: flatten against X/Y Horizontal plane.
    MiniMath::FVector Horizontal{HeadingReference.X, HeadingReference.Y, 0.0};
    const double HorizontalLen = Length(Horizontal);
    if (HorizontalLen > 0.0001)
    {
        const double Coefficient = std::sqrt(std::max(0.0, 1.0 - HorizontalLen));
        HeadingReference = HeadingReference * std::max(0.0, 1.0 - Coefficient) + Horizontal * (Coefficient / (HorizontalLen + Epsilon));
        HeadingReference = Normalized(HeadingReference, CarHeading);
    }

    PointVec = CameraAnchor - TargetPos;
    const double AnchorLen = Length(PointVec);
    CameraAnchor = AnchorLen > 0.0001
        ? TargetPos + PointVec * (Settings.CameraRelaxationCm / AnchorLen) : TargetPos - HeadingReference * Settings.CameraRelaxationCm;

    MiniMath::FVector Right = Cross(WorldUp(), HeadingReference);
    const double RightLen = Length(Right);
    CameraAnchorPerpendicular = RightLen > 0.0001
        ? TargetPos + Right * (-Settings.CameraRelaxationCm * 0.8 / RightLen) : TargetPos;

    LastTargetPosition = TargetPos;
}

void FBeamNGOrbitCameraMod::RequestRecenter(bool bKeepPitchAndDistance, const MiniMath::FVector& TargetPos, const MiniMath::FVector& CarHeading)
{
    MiniMath::FVector WorldForward = RotateAroundAxis(HeadingReference, WorldUp(), DisplayedYawRad);
    WorldForward = Normalized(WorldForward, CarHeading);

    InitializeHeading(TargetPos, CarHeading);
    DisplayedYawRad = SignedHeadingError(CarHeading, WorldForward);
    OrbitYawRad = 0.0;

    if (bKeepPitchAndDistance)
        OrbitPitchRad = DisplayedPitchRad;
    else
    {
        OrbitPitchRad = Settings.CameraPitchRad;
        OrbitDistanceCm = Settings.CameraDistanceCm;
    }

    bLockCamera = false;
    AccumulatedManualYawRad = 0.0;
    ResetCollisionState();
}

void FBeamNGOrbitCameraMod::UpdateDynamicPitchState(double Speed, bool bManualRotationActive, double DeltaTimeSeconds)
{
    if (Settings.DynamicPitchAtSpeedRad <= 0.0)
    {
        bAbovePitchSpeedThreshold = false;
        BelowPitchThresholdTimer = -1.0;
        DynamicPitchBlend = 0.0;
        DynamicPitchVelocity = 0.0;
        return;
    }

    if (bManualRotationActive) TimeSinceManualRotation = 0.0;
    else TimeSinceManualRotation += DeltaTimeSeconds;

    if (TimeSinceManualRotation <= DynamicPitchManualDelay) return;

    if (bAbovePitchSpeedThreshold)
    {
        if (Speed < DynamicPitchLowerSpeed)
        {
            if (BelowPitchThresholdTimer < 0.0)
                BelowPitchThresholdTimer = DynamicPitchBlend >= 0.995
                    ? DynamicPitchLowSpeedDelay : 0.0;

            BelowPitchThresholdTimer -= DeltaTimeSeconds;
            if (BelowPitchThresholdTimer <= 0.0)
            {
                bAbovePitchSpeedThreshold = false;
                BelowPitchThresholdTimer = -1.0;
            }
        }
        else BelowPitchThresholdTimer = -1.0;
    }
    else
    {
        bAbovePitchSpeedThreshold = Speed > DynamicPitchUpperSpeed;
    }

    const double target = bAbovePitchSpeedThreshold ? 1.0 : 0.0;
    const double rate = target > DynamicPitchBlend
        ? DynamicPitchRiseRate : DynamicPitchFallRate;
    RateAccelStep(DynamicPitchBlend, DynamicPitchVelocity, target, rate, rate, DeltaTimeSeconds);
}

double FBeamNGOrbitCameraMod::CalculateDynamicPitchLimit(const MiniMath::FVector& TargetPos,
    const MiniMath::FMatrix3& CarMatrix, const MiniMath::FVector& RearBottomPoint, double BaseFovDeg) const
{
    const MiniMath::FVector DefaultCameraOffsetLocal{
        0.0, -std::cos(Settings.CameraPitchRad) * Settings.CameraDistanceCm, std::sin(Settings.CameraPitchRad) * Settings.CameraDistanceCm
    };
    const MiniMath::FVector DefaultCameraOffsetWorld = Rotate(CarMatrix, DefaultCameraOffsetLocal);

    MiniMath::FVector ToRearBottom = RearBottomPoint - TargetPos - DefaultCameraOffsetWorld;
    MiniMath::FVector ToTarget = DefaultCameraOffsetWorld * -1.0;
    const double TargetLen = Length(ToTarget);
    const double RearLen = Length(ToRearBottom);
    if (TargetLen < 0.0001 || RearLen < 0.0001) return 0.0;

    ToTarget = ToTarget / TargetLen;
    ToRearBottom = ToRearBottom / RearLen;
    const double Separation = std::acos(std::clamp(Dot(ToTarget, ToRearBottom), -1.0, 1.0));
    return std::max(BaseFovDeg * 0.5 * DegToRad - Separation, 0.0);
}

void FBeamNGOrbitCameraMod::CalculateDynamicFovAndDistance(double TargetDistanceCm, double SmoothedDistanceCm, double Speed,
    const MiniMath::FVector& TargetPos, const MiniMath::FVector& RearReferencePoint, double& OutFov, double& OutDistanceCm) const
{
    const double DynamicFov = std::clamp(Settings.CameraFovDeg + Settings.DynamicFovAtSpeedDeg * std::min(1.0, Speed / DynamicFovSpeed), 10.0, 160.0);
    const double RefToRearCm = Length(RearReferencePoint - TargetPos);
    const double Ratio = std::tan(Settings.CameraFovDeg * Pi / 360.0) / std::tan(DynamicFov * Pi / 360.0);
    const double DistanceDelta = (TargetDistanceCm - RefToRearCm) * (Ratio - 1.0);

    OutFov = DynamicFov;
    OutDistanceCm = std::max(10.0, SmoothedDistanceCm + DistanceDelta);
}

double FBeamNGOrbitCameraMod::CalculateDynamicHeightCm(double Speed) const
{
    const double velocity = std::min(Speed, 70.0);
    const double SmoothedVelocity = std::max(velocity * 0.05 - 0.2, 0.0);
    const double LengthValue = std::min((1.4 * SmoothedVelocity) / (SmoothedVelocity + 4.1), 1.0);
    return LengthValue * Settings.DynamicHeightAtSpeedCm;
}

void FBeamNGOrbitCameraMod::ResetCameraState()
{
    bWasThirdFarCameraActive = false;
    bOrbitInitialized = false;
    OrbitYawRad = 0.0;
    OrbitPitchRad = 0.0;
    DisplayedYawRad = 0.0;
    DisplayedPitchRad = 0.0;
    OrbitDistanceCm = 0.0;
    DisplayedDistanceCm = 0.0;
    bLockCamera = false;
    AccumulatedManualYawRad = 0.0;
    LastAppliedFov = Settings.CameraFovDeg;

    HeadingReference = {0.0,1.0,0.0};
    LastValidCarHeading = {0.0,1.0,0.0};
    CameraAnchor = {};
    CameraAnchorPerpendicular = {};
    LastTargetPosition = {};
    bHeadingInitialized = false;
    bResetHeadingReference = false;

    TimeSinceManualRotation = 1000.0;
    bAbovePitchSpeedThreshold = false;
    BelowPitchThresholdTimer = -1.0;
    DynamicPitchBlend = 0.0;
    DynamicPitchVelocity = 0.0;

    PreviousCarPosition = {};
    bPreviousCarPositionValid = false;
    DynamicSpeedMps = 0.0;
    bDynamicSpeedInitialized = false;
    TargetHeightOffsetCm = Settings.TargetHeightOffsetCm;
#if BEAMNG_ORBIT_CAMERA_DIAGNOSTICS
    DiagnosticTime = 0.0;
#endif

    bNum5WasDown = false;

    bGamepadR3WasDown = false;
    bGamepadL3WasDown = false;

    if (!XInputGetState)
    {
        bGamepadConnected = false;
        GamepadUserIndex = -1;
    }

    ResetCollisionState();

    bOutputValid = false;
    LastComputedFrame = std::numeric_limits<uint64_t>::max();
    OutputLocation = {};
    OutputRotation = {};
    OutputFov = Settings.CameraFovDeg;
    PendingCleanPoseManager = nullptr;
    PendingCleanPose = {};
    PendingWrittenPose = {};
    bCleanPosePending = false;
    bLoggedFirstOutput = false;
    bLoggedFailure = false;
}

void FBeamNGOrbitCameraMod::LogFailureOnce(const CharType* Message)
{
    if (bLoggedFailure) return;
    bLoggedFailure = true;
    Output::send<LogLevel::Warning>(STR("[BeamNGOrbitCamera] Camera update failed: {}\n"), Message);
}


void FBeamNGOrbitCameraObjectCreateListener::NotifyUObjectCreated(const UObjectBase* Object, int32)
{
    if (Owner && Object) Owner->OnObjectCreated(std::bit_cast<UObject*>(Object));
}

void FBeamNGOrbitCameraObjectCreateListener::OnUObjectArrayShutdown()
{
    UObjectArray::RemoveUObjectCreateListener(this);
    if (Owner) Owner->OnObjectArrayShutdown();
}

void FBeamNGOrbitCameraObjectDeleteListener::NotifyUObjectDeleted(const UObjectBase* Object, int32)
{
    if (Owner && Object) Owner->OnObjectDeleted(std::bit_cast<UObject*>(Object));
}

void FBeamNGOrbitCameraObjectDeleteListener::OnUObjectArrayShutdown()
{
    UObjectArray::RemoveUObjectDeleteListener(this);
    if (Owner) Owner->OnObjectArrayShutdown();
}
