#include "BeamNGOrbitCameraMod.hpp"

#include "Core/MiniMath.hpp"
#include "Core/UnrealReflection.hpp"

#include <cmath>
#include <vector>

namespace MiniMath = BeamNGOrbitCamera::MiniMath;
using namespace BeamNGOrbitCamera::MiniMath;
using namespace BeamNGOrbitCamera::Reflection;

void FBeamNGOrbitCameraMod::InvalidateBodyGeometryCache()
{
    BodyOwner = nullptr;
    BodyComponent = nullptr;
    BodyMeshAsset = nullptr;
    BodyLocalOrigin = {};
    BodyLocalExtent = {};
    bBodyBoundsValid = false;
}

bool FBeamNGOrbitCameraMod::CacheBodyComponent(UObject* Car)
{
    if (!Car) return false;

    if (BodyOwner == Car && BodyComponent && BodyMeshAsset && bBodyBoundsValid)
    {
        return true;
    }

    InvalidateBodyGeometryCache();
    BodyOwner = Car;

    std::vector<UObject*> AllComponents{};
    UObjectGlobals::FindAllOf(STR("ActorComponent"), AllComponents);

    for (UObject* Component : AllComponents)
    {
        if (!Component || Component->GetName() != STR("Body_Component") || !ObjectOuterChainContains(Component, Car))
        {
            continue;
        }

        UObject* MeshAsset = nullptr;
        static constexpr const CharType* MeshPropertyNames[] = {
            STR("SkeletalMeshAsset"), STR("SkeletalMesh"), STR("SkinnedAsset")
        };

        for (const CharType* PropertyName : MeshPropertyNames)
        {
            MeshAsset = ReadObjectPropertyByName(Component, PropertyName);
            if (MeshAsset) break;
        }

        if (!MeshAsset) continue;

        MiniMath::FVector LocalOrigin{};
        MiniMath::FVector LocalExtent{};
        double SphereRadius = 0.0;
        if (!ReadMeshAssetBounds(MeshAsset, LocalOrigin, LocalExtent, SphereRadius))
        {
            continue;
        }

        if (LocalExtent.X <= 1.0 || LocalExtent.Y <= 1.0 || LocalExtent.Z <= 1.0)
        {
            continue;
        }

        BodyComponent = Component;
        BodyMeshAsset = MeshAsset;
        BodyLocalOrigin = LocalOrigin;
        BodyLocalExtent = LocalExtent;
        bBodyBoundsValid = true;

        return true;
    }

    return false;
}

bool FBeamNGOrbitCameraMod::ResolveBodyGeometry(UObject* Car, FBodyGeometry& OutGeometry)
{
    if (!CacheBodyComponent(Car))
        return false;

    MiniMath::FVector ComponentLocation{};
    MiniMath::FRotator ComponentRotation{};
    MiniMath::FVector ComponentScale{1.0, 1.0, 1.0};

    const bool bHasLocation = CallNoArgStructFunction(BodyComponent, STR("K2_GetComponentLocation"), [&](const FStructAccess& Result) {
                return ReadVec3(Result, ComponentLocation);
            }
        );

    const bool bHasRotation = CallNoArgStructFunction(BodyComponent, STR("K2_GetComponentRotation"), [&](const FStructAccess& Result) {
                return ReadRot3(Result, ComponentRotation);
            }
        );

    CallNoArgStructFunction(BodyComponent, STR("K2_GetComponentScale"), [&](const FStructAccess& Result) {
            return ReadVec3(Result, ComponentScale);
        }
    );

    if (!bHasLocation || !bHasRotation)
        return false;

    const MiniMath::FMatrix3 BodyMatrix = RotatorToMatrix(ComponentRotation);

    const MiniMath::FVector ScaledLocalOrigin{
        BodyLocalOrigin.X * ComponentScale.X, BodyLocalOrigin.Y * ComponentScale.Y, BodyLocalOrigin.Z * ComponentScale.Z
    };

    OutGeometry.Center = ComponentLocation
        + Rotate(BodyMatrix, ScaledLocalOrigin);

    OutGeometry.Extent = {
        BodyLocalExtent.X * std::abs(ComponentScale.X), BodyLocalExtent.Y * std::abs(ComponentScale.Y), BodyLocalExtent.Z * std::abs(ComponentScale.Z)
    };

    OutGeometry.Forward = Normalized(MatrixVehicleForward(BodyMatrix), MiniMath::FVector{0.0, 1.0, 0.0});

    OutGeometry.Up = Normalized(MatrixVehicleUp(BodyMatrix), WorldUp());


    return true;
}

