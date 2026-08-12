#include "Core/UnrealReflection.hpp"

#include <cmath>

namespace MiniMath = BeamNGOrbitCamera::MiniMath;
namespace BeamNGOrbitCamera::Reflection
{
    UObject* ResolveViewTargetActor(UObject* CameraManager)
    {
        if (!CameraManager)
        {
            return nullptr;
        }

        auto* ViewTargetProperty = CastField<FStructProperty>(CameraManager
                    ->GetPropertyByNameInChain(STR("ViewTarget")));

        if (!ViewTargetProperty)
        {
            return nullptr;
        }

        UStruct* ViewTargetType = ViewTargetProperty->GetStruct();

        void* ViewTargetValue = ViewTargetProperty
                ->ContainerPtrToValuePtr<void>(CameraManager);

        if (!ViewTargetType || !ViewTargetValue)
        {
            return nullptr;
        }

        FProperty* TargetProperty = ViewTargetType
                ->GetPropertyByNameInChain(STR("Target"));

        if (!TargetProperty)
        {
            return nullptr;
        }

        UObject** Target = TargetProperty
                ->ContainerPtrToValuePtr<UObject*>(ViewTargetValue);

        return Target
                ? *Target : nullptr;
    }

    double ReadViewTargetAspectRatio(UObject* CameraManager)
    {
        constexpr double FallbackAspect = 16.0 / 9.0;

        if (!CameraManager)
        {
            return FallbackAspect;
        }

        auto* ViewTargetProperty = CastField<FStructProperty>(CameraManager
                    ->GetPropertyByNameInChain(STR("ViewTarget")));

        if (!ViewTargetProperty)
        {
            return FallbackAspect;
        }

        UStruct* ViewTargetType = ViewTargetProperty->GetStruct();

        void* ViewTargetValue = ViewTargetProperty
                ->ContainerPtrToValuePtr<void>(CameraManager);

        if (!ViewTargetType || !ViewTargetValue)
        {
            return FallbackAspect;
        }

        auto* PovProperty = CastField<FStructProperty>(ViewTargetType
                    ->GetPropertyByNameInChain(STR("POV")));

        if (!PovProperty)
        {
            return FallbackAspect;
        }

        UStruct* PovType = PovProperty->GetStruct();

        void* PovValue = PovProperty
                ->ContainerPtrToValuePtr<void>(ViewTargetValue);

        if (!PovType || !PovValue)
        {
            return FallbackAspect;
        }

        FProperty* AspectProperty = PovType
                ->GetPropertyByNameInChain(STR("AspectRatio"));

        double Aspect = FallbackAspect;

        if (!ReadScalar(AspectProperty, PovValue, Aspect))
        {
            return FallbackAspect;
        }

        if (!std::isfinite(Aspect) || Aspect < 0.5 || Aspect > 5.0)
        {
            return FallbackAspect;
        }

        return Aspect;
    }

    bool ObjectOuterChainContains(UObject* Object, UObject* WantedOuter)
    {
        if (!Object || !WantedOuter)
        {
            return false;
        }

        UObject* OuterObject = Object->GetOuterPrivate();

        int32_t Depth = 0;

        while (OuterObject && Depth < 32)
        {
            if (OuterObject == WantedOuter)
            {
                return true;
            }

            OuterObject = OuterObject->GetOuterPrivate();

            ++Depth;
        }

        return false;
    }

    UObject* ReadObjectPropertyByName(UObject* Object, const CharType* PropertyName)
    {
        if (!Object)
        {
            return nullptr;
        }

        FProperty* Property = Object->GetPropertyByNameInChain(PropertyName);

        if (!Property)
        {
            return nullptr;
        }

        UObject** Value = Property
                ->ContainerPtrToValuePtr<UObject*>(Object);

        return Value
                ? *Value : nullptr;
    }

    bool CallNoArgObjectFunction(UObject* Context, const CharType* FunctionName, UObject*& OutObject)
    {
        OutObject = nullptr;

        if (!Context)
        {
            return false;
        }

        UFunction* Function = Context->GetFunctionByNameInChain(FromCharTypePtr<TCHAR>(FunctionName));

        if (!Function)
        {
            return false;
        }

        FProperty* ReturnProperty = nullptr;

        for (FProperty* Property :
             TFieldRange<FProperty>(Function, EFieldIterationFlags::IncludeDeprecated))
        {
            if (Property->HasAnyPropertyFlags(CPF_ReturnParm))
            {
                ReturnProperty = Property;
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

        UObject** Result = ReturnProperty
                ->ContainerPtrToValuePtr<UObject*>(Params.data());

        if (!Result)
        {
            return false;
        }

        OutObject = *Result;
        return true;
    }

    bool CallTwoOutVec3Function(UObject* Context, const CharType* FunctionName, const CharType* FirstName,
        MiniMath::FVector& First, const CharType* SecondName, MiniMath::FVector& Second)
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

        auto* FirstProperty = CastField<FStructProperty>(Function
                    ->GetPropertyByNameInChain(FirstName));

        auto* SecondProperty = CastField<FStructProperty>(Function
                    ->GetPropertyByNameInChain(SecondName));

        if (!FirstProperty || !SecondProperty)
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

        FStructAccess FirstAccess{};
        FirstAccess.Property = FirstProperty;
        FirstAccess.Type = FirstProperty->GetStruct();
        FirstAccess.Value = FirstProperty
                ->ContainerPtrToValuePtr<void>(Params.data());

        FStructAccess SecondAccess{};
        SecondAccess.Property = SecondProperty;
        SecondAccess.Type = SecondProperty->GetStruct();
        SecondAccess.Value = SecondProperty
                ->ContainerPtrToValuePtr<void>(Params.data());

        return FirstAccess.Type && FirstAccess.Value && SecondAccess.Type && SecondAccess.Value && ReadVec3(FirstAccess, First
            ) && ReadVec3(SecondAccess, Second);
    }

    bool ReadBoxSphereBounds(const FStructAccess& Bounds, MiniMath::FVector& Origin, MiniMath::FVector& Extent, double& SphereRadius)
    {
        if (!Bounds.Type || !Bounds.Value)
        {
            return false;
        }

        auto* OriginProperty = CastField<FStructProperty>(Bounds.Type
                    ->GetPropertyByNameInChain(STR("Origin")));

        auto* ExtentProperty = CastField<FStructProperty>(Bounds.Type
                    ->GetPropertyByNameInChain(STR("BoxExtent")));

        FProperty* RadiusProperty = Bounds.Type
                ->GetPropertyByNameInChain(STR("SphereRadius"));

        if (!OriginProperty || !ExtentProperty || !RadiusProperty)
        {
            return false;
        }

        FStructAccess OriginAccess{};
        OriginAccess.Property = OriginProperty;
        OriginAccess.Type = OriginProperty->GetStruct();
        OriginAccess.Value = OriginProperty
                ->ContainerPtrToValuePtr<void>(Bounds.Value);

        FStructAccess ExtentAccess{};
        ExtentAccess.Property = ExtentProperty;
        ExtentAccess.Type = ExtentProperty->GetStruct();
        ExtentAccess.Value = ExtentProperty
                ->ContainerPtrToValuePtr<void>(Bounds.Value);

        return OriginAccess.Type && OriginAccess.Value && ExtentAccess.Type && ExtentAccess.Value && ReadVec3(OriginAccess, Origin
            ) && ReadVec3(ExtentAccess, Extent) && ReadScalar(RadiusProperty, Bounds.Value, SphereRadius);
    }

    bool ReadMeshAssetBounds(UObject* Asset, MiniMath::FVector& Origin, MiniMath::FVector& Extent, double& SphereRadius)
    {
        if (!Asset)
        {
            return false;
        }

        UFunction* Function = Asset->GetFunctionByNameInChain(FromCharTypePtr<TCHAR>(STR("GetBounds")));

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

        Asset->ProcessEvent(Function, Params.data());

        FStructAccess Bounds{};
        Bounds.Property = ReturnProperty;
        Bounds.Type = ReturnProperty->GetStruct();
        Bounds.Value = ReturnProperty
                ->ContainerPtrToValuePtr<void>(Params.data());

        return ReadBoxSphereBounds(Bounds, Origin, Extent, SphereRadius);
    }

    bool CallActorLocation(UObject* Actor, MiniMath::FVector& OutLocation)
    {
        return CallNoArgStructFunction(Actor, STR("K2_GetActorLocation"), [&](const FStructAccess& Result) {
                return ReadVec3(Result, OutLocation);
            }
        );
    }

    bool CallActorRotation(UObject* Actor, MiniMath::FRotator& OutRotation)
    {
        return CallNoArgStructFunction(Actor, STR("K2_GetActorRotation"), [&](const FStructAccess& Result) {
                return ReadRot3(Result, OutRotation);
            }
        );
    }

    bool CallActorVelocity(UObject* Actor, MiniMath::FVector& OutVelocity)
    {
        return CallNoArgStructFunction(Actor, STR("GetVelocity"), [&](const FStructAccess& Result) {
                return ReadVec3(Result, OutVelocity);
            }
        );
    }

    bool ResolveStructParam(UFunction* Function, void* Params, const CharType* Name, FStructAccess& OutAccess)
    {
        if (!Function || !Params)
        {
            return false;
        }

        auto* Property = CastField<FStructProperty>(Function
                    ->GetPropertyByNameInChain(Name));

        if (!Property)
        {
            return false;
        }

        OutAccess.Property = Property;
        OutAccess.Type = Property->GetStruct();
        OutAccess.Value = Property
                ->ContainerPtrToValuePtr<void>(Params);

        return OutAccess.Type && OutAccess.Value;
    }

    bool ReadVec3(const FStructAccess& Access, MiniMath::FVector& OutVector)
    {
        return ReadNamedScalar(Access, STR("X"), OutVector.X) && ReadNamedScalar(Access,
                STR("Y"), OutVector.Y) && ReadNamedScalar(Access, STR("Z"), OutVector.Z);
    }

    bool WriteVec3(const FStructAccess& Access, const MiniMath::FVector& Vector)
    {
        return WriteNamedScalar(Access, STR("X"), Vector.X) && WriteNamedScalar(Access, STR("Y"), Vector.Y) && WriteNamedScalar(Access, STR("Z"), Vector.Z);
    }

    bool ReadRot3(const FStructAccess& Access, MiniMath::FRotator& OutRotator)
    {
        return ReadNamedScalar(Access, STR("Pitch"), OutRotator.Pitch) && ReadNamedScalar(Access,
                STR("Yaw"), OutRotator.Yaw) && ReadNamedScalar(Access, STR("Roll"), OutRotator.Roll);
    }

    bool WriteRot3(const FStructAccess& Access, const MiniMath::FRotator& Rotator)
    {
        return WriteNamedScalar(Access, STR("Pitch"), Rotator.Pitch) && WriteNamedScalar(Access,
                STR("Yaw"), Rotator.Yaw) && WriteNamedScalar(Access, STR("Roll"), Rotator.Roll);
    }

    bool ReadNamedScalar(const FStructAccess& Access, const CharType* Name, double& OutValue)
    {
        if (!Access.Type || !Access.Value)
        {
            return false;
        }

        return ReadScalar(Access.Type
                ->GetPropertyByNameInChain(Name), Access.Value, OutValue);
    }

    bool WriteNamedScalar(const FStructAccess& Access, const CharType* Name, double Value)
    {
        if (!Access.Type || !Access.Value)
        {
            return false;
        }

        return WriteScalar(Access.Type
                ->GetPropertyByNameInChain(Name), Access.Value, Value);
    }

    bool ReadScalar(FProperty* Property, void* Container, double& OutValue)
    {
        if (!Property || !Container)
        {
            return false;
        }

        const auto ClassName = Property->GetClass().GetName();

        if (ClassName == STR("DoubleProperty"))
        {
            auto* Value = Property
                    ->ContainerPtrToValuePtr<double>(Container);

            if (!Value)
            {
                return false;
            }

            OutValue = *Value;
            return true;
        }

        if (ClassName == STR("FloatProperty"))
        {
            auto* Value = Property->ContainerPtrToValuePtr<float>(Container);

            if (!Value)
            {
                return false;
            }

            OutValue = static_cast<double>(
                    *Value
                );

            return true;
        }

        return false;
    }

    bool WriteScalar(FProperty* Property, void* Container, double Value)
    {
        if (!Property || !Container)
        {
            return false;
        }

        const auto ClassName = Property->GetClass().GetName();

        if (ClassName == STR("DoubleProperty"))
        {
            auto* Destination = Property
                    ->ContainerPtrToValuePtr<double>(Container);

            if (!Destination)
            {
                return false;
            }

            *Destination = Value;
            return true;
        }

        if (ClassName == STR("FloatProperty"))
        {
            auto* Destination = Property
                    ->ContainerPtrToValuePtr<float>(Container);

            if (!Destination)
            {
                return false;
            }

            *Destination = static_cast<float>(Value);

            return true;
        }

        return false;
    }
}
