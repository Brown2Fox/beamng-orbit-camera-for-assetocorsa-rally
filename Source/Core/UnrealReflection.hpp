#pragma once

#include <cstdint>
#include <vector>

#include <Helpers/String.hpp>

#include <Unreal/UObject.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>

#include "Core/CameraTypes.hpp"

using namespace RC;
using namespace RC::Unreal;

namespace BeamNGOrbitCamera::Reflection
{
    UObject* ResolveViewTargetActor(UObject* CameraManager);

    double ReadViewTargetAspectRatio(UObject* CameraManager);

    bool ObjectOuterChainContains(UObject* Object, UObject* WantedOuter);

    UObject* ReadObjectPropertyByName(UObject* Object, const CharType* PropertyName);

    bool CallNoArgObjectFunction(UObject* Context, const CharType* FunctionName, UObject*& OutObject);

    bool CallTwoOutVec3Function(UObject* Context, const CharType* FunctionName, const CharType* FirstName,
        MiniMath::FVector& First, const CharType* SecondName, MiniMath::FVector& Second);

    bool ReadBoxSphereBounds(const FStructAccess& Bounds, MiniMath::FVector& Origin, MiniMath::FVector& Extent,
        double& SphereRadius);

    bool ReadMeshAssetBounds(UObject* Asset, MiniMath::FVector& Origin, MiniMath::FVector& Extent, double& SphereRadius);

    bool CallActorLocation(UObject* Actor, MiniMath::FVector& OutLocation);

    bool CallActorRotation(UObject* Actor, MiniMath::FRotator& OutRotation);

    bool CallActorVelocity(UObject* Actor, MiniMath::FVector& OutVelocity);

    template <typename TReader>
    bool CallNoArgStructFunction(UObject* Context, const CharType* FunctionName, TReader&& Reader)
    {
        if (!Context)
        {
            return false;
        }

        UFunction* Function = Context->GetFunctionByNameInChain(FromCharTypePtr<TCHAR>(FunctionName));

        if (!Function)
        {
            return false;
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
            return false;
        }

        const int32_t Size = Function->GetPropertiesSize();

        if (Size <= 0)
        {
            return false;
        }

        std::vector<uint8_t> Params(static_cast<size_t>(Size), uint8_t{0});

        Context->ProcessEvent(Function, Params.data());

        FStructAccess Result{};
        Result.Property = ReturnProperty;
        Result.Type = ReturnProperty->GetStruct();
        Result.Value = ReturnProperty
                ->ContainerPtrToValuePtr<void>(Params.data());

        return Result.Type && Result.Value && Reader(Result);
    }

    bool ResolveStructParam(UFunction* Function, void* Params, const CharType* Name, FStructAccess& OutAccess);

    bool ReadVec3(const FStructAccess& Access, MiniMath::FVector& OutVector);

    bool WriteVec3(const FStructAccess& Access, const MiniMath::FVector& Vector);

    bool ReadRot3(const FStructAccess& Access, MiniMath::FRotator& OutRotator);

    bool WriteRot3(const FStructAccess& Access, const MiniMath::FRotator& Rotator);

    bool ReadNamedScalar(const FStructAccess& Access, const CharType* Name, double& OutValue);

    bool WriteNamedScalar(const FStructAccess& Access, const CharType* Name, double Value);

    bool ReadScalar(FProperty* Property, void* Container, double& OutValue);

    bool WriteScalar(FProperty* Property, void* Container, double Value);
}
