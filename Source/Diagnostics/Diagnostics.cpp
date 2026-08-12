#include "BeamNGOrbitCameraMod.hpp"

#include "Core/UnrealReflection.hpp"

#include <cmath>
#include <vector>

using namespace BeamNGOrbitCamera::Reflection;
namespace MiniMath = BeamNGOrbitCamera::MiniMath;

#if BEAMNG_ORBIT_CAMERA_DIAGNOSTICS

namespace
{
    void DumpObjectReference(const CharType* Label, UObject* Object)
    {
        if (!Object)
        {
            Output::send<LogLevel::Verbose>(STR("[BeamNGOrbitCamera] {}: <null>\n"), Label);
            return;
        }

        Output::send<LogLevel::Verbose>(STR("[BeamNGOrbitCamera] {}: {}\n"), Label, Object->GetFullName());

        if (auto* Class = Object->GetClassPrivate())
            Output::send<LogLevel::Verbose>(STR("[BeamNGOrbitCamera] {}Class: {}\n"), Label, Class->GetFullName());
    }

    void DumpPropertyType(UObject* Object, const CharType* Name)
    {
        FProperty* Property = Object ? Object->GetPropertyByNameInChain(Name) : nullptr;
        if (Property)
            Output::send<LogLevel::Verbose>(STR("      {} : {}\n"), Name, Property->GetClass().GetName());
        else
            Output::send<LogLevel::Verbose>(STR("      {} : <not reflected>\n"), Name);
    }

    void DumpNameProperty(UObject* Object, const CharType* Name)
    {
        FProperty* Property = Object ? Object->GetPropertyByNameInChain(Name) : nullptr;
        if (!Property || Property->GetClass().GetName() != STR("NameProperty"))
            return;

        FName* Value = Property->ContainerPtrToValuePtr<FName>(Object);
        if (Value)
            Output::send<LogLevel::Verbose>(STR("[BeamNGOrbitCamera] {}: {}\n"), Name, Value->ToString());
    }
}

void FBeamNGOrbitCameraMod::DumpCameraManager()
{
    if (!CameraManager)
    {
        Output::send<LogLevel::Warning>(STR("[BeamNGOrbitCamera] CAMERA MANAGER DUMP: manager is null.\n"));
        return;
    }

    Output::send<LogLevel::Verbose>(STR("\n[BeamNGOrbitCamera] ==================== CAMERA MANAGER DUMP BEGIN ====================\n"));
    DumpObjectReference(STR("Manager"), CameraManager);

    UObject* CurrentCamera = nullptr;
    if (CallNoArgObjectFunction(CameraManager, STR("GetCurrentCamera"), CurrentCamera))
        DumpObjectReference(STR("CurrentCamera"), CurrentCamera);
    else
        Output::send<LogLevel::Verbose>(STR("[BeamNGOrbitCamera] GetCurrentCamera: unavailable\n"));

    UObject* DebugCamera = nullptr;
    if (CallNoArgObjectFunction(CameraManager, STR("GetDebugCamera"), DebugCamera))
        DumpObjectReference(STR("DebugCamera"), DebugCamera);

    UObject* CarAvatar = nullptr;
    if (CallNoArgObjectFunction(CameraManager, STR("GetCarAvatar"), CarAvatar))
        DumpObjectReference(STR("CarAvatar"), CarAvatar);

    DumpObjectReference(STR("ActiveCameraHandler"), ReadObjectPropertyByName(CameraManager, STR("ActiveCameraHandler")));
    DumpObjectReference(STR("DebugCameraActor"), ReadObjectPropertyByName(CameraManager, STR("DebugCameraActor")));
    DumpObjectReference(STR("ViewTarget"), ResolveViewTargetActor(CameraManager));

    Output::send<LogLevel::Verbose>(STR("[BeamNGOrbitCamera] Reflected manager fields:\n"));
    DumpPropertyType(CameraManager, STR("DefaultGameplayCamera"));
    DumpPropertyType(CameraManager, STR("ActiveCameraHandler"));
    DumpPropertyType(CameraManager, STR("DebugCameraActor"));
    DumpPropertyType(CameraManager, STR("CameraStyle"));
    DumpPropertyType(CameraManager, STR("ViewTarget"));
    DumpPropertyType(CameraManager, STR("PendingViewTarget"));
    DumpNameProperty(CameraManager, STR("CameraStyle"));

    Output::send<LogLevel::Verbose>(STR("[BeamNGOrbitCamera] ===================== CAMERA MANAGER DUMP END =====================\n\n"));
}

void FBeamNGOrbitCameraMod::DumpMeshAssetBounds(UObject* Asset)
{
    if (!Asset) return;

    UFunction* Function = Asset->GetFunctionByNameInChain(FromCharTypePtr<TCHAR>(STR("GetBounds")));
    if (!Function)
    {
        Output::send<LogLevel::Verbose>(STR("      AssetBounds: GetBounds() not reflected\\n"));
        return;
    }

    FStructProperty* ReturnProperty = nullptr;
    for (FProperty* Property :
         TFieldRange<FProperty>(Function, EFieldIterationFlags::IncludeDeprecated))
    {
        if (Property->HasAnyPropertyFlags(CPF_ReturnParm))
        {
            ReturnProperty = CastField<FStructProperty>(Property);
            break;
        }
    }
    if (!ReturnProperty)
    {
        Output::send<LogLevel::Verbose>(STR("      AssetBounds: no struct return\\n"));
        return;
    }

    const int32_t Size = Function->GetPropertiesSize();
    if (Size <= 0) return;

    std::vector<uint8_t> Params(static_cast<size_t>(Size), uint8_t{0});
    Asset->ProcessEvent(Function, Params.data());

    FStructAccess Bounds{};
    Bounds.Property = ReturnProperty;
    Bounds.Type = ReturnProperty->GetStruct();
    Bounds.Value = ReturnProperty->ContainerPtrToValuePtr<void>(Params.data());

    MiniMath::FVector Origin{};
    MiniMath::FVector Extent{};
    double Radius = 0.0;
    if (ReadBoxSphereBounds(Bounds, Origin, Extent, Radius))
    {
        Output::send<LogLevel::Verbose>(STR("      AssetBounds local: Origin=({:.1f},{:.1f},{:.1f}) Extent=({:.1f},{:.1f},{:.1f})cm Radius={:.1f}cm\\n"),
            Origin.X, Origin.Y, Origin.Z, Extent.X, Extent.Y, Extent.Z, Radius);
    }
    else
    {
        Output::send<LogLevel::Verbose>(STR("      AssetBounds: returned struct not decoded\\n"));
    }
}

void FBeamNGOrbitCameraMod::DumpVehicleComponents(UObject* Car)
{
    Output::send<LogLevel::Verbose>(STR("\\n[BeamNGOrbitCamera] ==================== VEHICLE COMPONENT CENSUS BEGIN ====================\\n"));
    Output::send<LogLevel::Verbose>(STR("[BeamNGOrbitCamera] Vehicle: {}\\n"), Car->GetFullName());

    auto* CarClass = Car->GetClassPrivate();
    if (CarClass)
    {
        Output::send<LogLevel::Verbose>(STR("[BeamNGOrbitCamera] VehicleClass: {}\\n"), CarClass->GetFullName());
    }

    std::vector<UObject*> AllComponents{};
    UObjectGlobals::FindAllOf(STR("ActorComponent"), AllComponents);

    size_t OwnedCount = 0;
    size_t GlobalCount = AllComponents.size();

    for (UObject* Component : AllComponents)
    {
        if (!Component)
            continue;

        if (!ObjectOuterChainContains(Component, Car))
            continue;

        ++OwnedCount;

        auto* ComponentClass = Component->GetClassPrivate();
        UObject* OuterObject = Component->GetOuterPrivate();

        Output::send<LogLevel::Verbose>(STR("[BeamNGOrbitCamera] COMPONENT #{:03}\\n"), OwnedCount);
        Output::send<LogLevel::Verbose>(STR("      Object: {}\\n"), Component->GetFullName());

        if (ComponentClass)
        {
            Output::send<LogLevel::Verbose>(STR("      Class:  {}\\n"), ComponentClass->GetFullName());

            if (auto* SuperClass = ComponentClass->GetSuperClass())
            {
                Output::send<LogLevel::Verbose>(STR("      Super:  {}\\n"), SuperClass->GetFullName());
            }
        }

        if (OuterObject)
        {
            Output::send<LogLevel::Verbose>(STR("      Outer:  {}\\n"), OuterObject->GetFullName());
        }

        UObject* InOwner = nullptr;
        if (CallNoArgObjectFunction(Component, STR("GetOwner"), InOwner) && InOwner)
        {
            Output::send<LogLevel::Verbose>(STR("      Owner:  {}\\n"), InOwner->GetFullName());
        }

        UObject* AttachParent = nullptr;
        if (CallNoArgObjectFunction(Component, STR("GetAttachParent"), AttachParent) && AttachParent)
        {
            Output::send<LogLevel::Verbose>(STR("      Parent: {}\\n"), AttachParent->GetFullName());
        }

        MiniMath::FVector location{};
        MiniMath::FRotator rotation{};
        const bool bHasLocation = CallNoArgStructFunction(Component, STR("K2_GetComponentLocation"), [&](const FStructAccess& Result) {
                    return ReadVec3(Result, location);
                }
            );
        const bool bHasRotation = CallNoArgStructFunction(Component, STR("K2_GetComponentRotation"), [&](const FStructAccess& Result) {
                    return ReadRot3(Result, rotation);
                }
            );

        if (bHasLocation)
        {
            Output::send<LogLevel::Verbose>(STR("      WorldLocation: ({:.1f},{:.1f},{:.1f})cm\\n"), location.X, location.Y, location.Z);
        }
        if (bHasRotation)
        {
            Output::send<LogLevel::Verbose>(STR("      WorldRotation: P={:.1f} Y={:.1f} R={:.1f}deg\\n"), rotation.Pitch, rotation.Yaw, rotation.Roll);
        }

        static constexpr const CharType* MeshPropertyNames[] = {
            STR("StaticMesh"), STR("SkeletalMeshAsset"), STR("SkeletalMesh"), STR("SkinnedAsset")
        };

        UObject* MeshAsset = nullptr;
        const CharType* MeshPropertyUsed = nullptr;

        for (const CharType* PropertyName : MeshPropertyNames)
        {
            UObject* Candidate = ReadObjectPropertyByName(Component, PropertyName);
            if (Candidate)
            {
                MeshAsset = Candidate;
                MeshPropertyUsed = PropertyName;
                break;
            }
        }

        if (MeshAsset)
        {
            Output::send<LogLevel::Verbose>(STR("      MeshAsset[{}]: {}\\n"), MeshPropertyUsed, MeshAsset->GetFullName());
            DumpMeshAssetBounds(MeshAsset);
        }

        MiniMath::FVector LocalMin{};
        MiniMath::FVector LocalMax{};
        if (CallTwoOutVec3Function(Component, STR("GetLocalBounds"), STR("Min"), LocalMin, STR("Max"), LocalMax))
        {
            const MiniMath::FVector LocalCenter = (LocalMin + LocalMax) * 0.5;
            const MiniMath::FVector LocalExtent = (LocalMax - LocalMin) * 0.5;

            Output::send<LogLevel::Verbose>(
                STR("      ComponentLocalBounds: min=({:.1f},{:.1f},{:.1f}) max=({:.1f},{:.1f},{:.1f}) ")
                STR("center=({:.1f},{:.1f},{:.1f}) Extent=({:.1f},{:.1f},{:.1f})cm\\n"),
                LocalMin.X, LocalMin.Y, LocalMin.Z, LocalMax.X, LocalMax.Y, LocalMax.Z,
                LocalCenter.X, LocalCenter.Y, LocalCenter.Z, LocalExtent.X, LocalExtent.Y, LocalExtent.Z);
        }
    }

    Output::send<LogLevel::Verbose>(STR("[BeamNGOrbitCamera] Census summary: owned={} / global ActorComponent objects scanned={}\\n"), OwnedCount, GlobalCount);
    Output::send<LogLevel::Verbose>(STR("[BeamNGOrbitCamera] ===================== VEHICLE COMPONENT CENSUS END =====================\\n\\n"));

    if (OwnedCount == 0)
    {
        Output::send<LogLevel::Warning>(
            STR("[BeamNGOrbitCamera] Census found zero components in the vehicle OuterObject chain. If ACR keeps components outside the ")
            STR("Actor OuterObject chain, ")
            STR("the bNextEnabled probe will switch To K2_GetComponentsByClass/GetOwner filtering.\\n")
        );
    }
}



#endif
