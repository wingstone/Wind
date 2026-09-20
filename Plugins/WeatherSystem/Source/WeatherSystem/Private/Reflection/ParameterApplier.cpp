// Copyright TADemo. All Rights Reserved.

#include "Reflection/ParameterApplier.h"

#include "Data/WeatherParamSchema.h"
#include "GameFramework/Actor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeatherApplier, Log, All);

namespace
{
	/** Find a component on Host by tag/name (tag preferred). NAME_None returns the first component of ClassT. */
	template <typename ClassT>
	ClassT* FindComponentByTagOrName(AActor* Host, FName Tag)
	{
		if (!Host) return nullptr;
		TArray<ClassT*> Comps;
		Host->GetComponents<ClassT>(Comps);
		if (Comps.Num() == 0) return nullptr;
		if (Tag.IsNone()) return Comps[0];
		for (ClassT* C : Comps)
		{
			if (!C) continue;
			if (C->ComponentHasTag(Tag) || C->GetFName() == Tag)
			{
				return C;
			}
		}
		return nullptr;
	}
}

void FParameterApplier::ResolveBindings(AActor* Host, const UWeatherParamSchema* Schema)
{
	Resolved.Reset();
	if (!Schema) return;
	const TArray<FWeatherParamBinding>& InBindings = Schema->Bindings;
	Resolved.SetNum(InBindings.Num());
	for (int32 i = 0; i < InBindings.Num(); ++i)
	{
		FResolvedBinding& R = Resolved[i];
		R.ParamName = InBindings[i].ParamName;
		R.Target    = InBindings[i].Target;
		R.Type      = InBindings[i].Type;
		R.bValid    = ResolveOne(Host, InBindings[i], Schema, R);
	}
}

void FParameterApplier::ResolveBindingsRaw(AActor* Host, const TArray<FWeatherParamBinding>& InBindings)
{
	Resolved.Reset();
	Resolved.SetNum(InBindings.Num());
	for (int32 i = 0; i < InBindings.Num(); ++i)
	{
		FResolvedBinding& R = Resolved[i];
		R.ParamName = InBindings[i].ParamName;
		R.Target    = InBindings[i].Target;
		R.Type      = InBindings[i].Type;
		R.bValid    = ResolveOne(Host, InBindings[i], nullptr, R);
	}
}

bool FParameterApplier::ResolveOne(AActor* Host, const FWeatherParamBinding& B, const UWeatherParamSchema* Schema, FResolvedBinding& Out) const
{
	if (!Host) return false;

	switch (B.Target)
	{
	case EWeatherParamTarget::ActorProperty:
	{
		// If a component tag is set, resolve the property on that component; otherwise on the actor itself.
		UObject* Owner = Host;
		if (!B.ComponentTag.IsNone())
		{
			if (UActorComponent* Comp = FindComponentByTagOrName<UActorComponent>(Host, B.ComponentTag))
			{
				Owner = Comp;
			}
			else
			{
				UE_LOG(LogWeatherApplier, Warning, TEXT("Component (tag=%s) not found on %s for property '%s'"),
					*B.ComponentTag.ToString(), *Host->GetName(), *B.ParamName.ToString());
				return false;
			}
		}
		FProperty* P = FindFProperty<FProperty>(Owner->GetClass(), B.ParamName);
		if (!P)
		{
			UE_LOG(LogWeatherApplier, Warning, TEXT("Property '%s' not found on %s"), *B.ParamName.ToString(), *Owner->GetName());
			return false;
		}
		Out.Property = P;
		Out.PropertyOwner = Owner;
		return true;
	}
	case EWeatherParamTarget::ActorFunction:
	{
		UObject* Owner = Host;
		if (!B.ComponentTag.IsNone())
		{
			if (UActorComponent* Comp = FindComponentByTagOrName<UActorComponent>(Host, B.ComponentTag))
			{
				Owner = Comp;
			}
			else
			{
				UE_LOG(LogWeatherApplier, Warning, TEXT("Component (tag=%s) not found on %s for function '%s'"),
					*B.ComponentTag.ToString(), *Host->GetName(), *B.ParamName.ToString());
				return false;
			}
		}
		UFunction* F = Owner->FindFunction(B.ParamName);
		if (!F)
		{
			UE_LOG(LogWeatherApplier, Warning, TEXT("Function '%s' not found on %s"), *B.ParamName.ToString(), *Owner->GetName());
			return false;
		}
		Out.Function = F;
		Out.FunctionOwner = Owner;
		return true;
	}
	case EWeatherParamTarget::MaterialParameter:
	{
		// Mesh path: dynamic material on element 0.
		if (UMeshComponent* Mesh = FindComponentByTagOrName<UMeshComponent>(Host, B.ComponentTag))
		{
			UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0);
			if (!MID)
			{
				UE_LOG(LogWeatherApplier, Warning, TEXT("Failed to create MID on %s"), *Mesh->GetName());
				return false;
			}
			Out.MID = MID;
			Out.MIDElementIndex = 0;
			return true;
		}

		// Light function path: dynamic material on the light's LightFunctionMaterial.
		if (ULightComponent* Light = FindComponentByTagOrName<ULightComponent>(Host, B.ComponentTag))
		{
			if (!Light->LightFunctionMaterial)
			{
				UE_LOG(LogWeatherApplier, Warning, TEXT("Light %s has no light function material set; binding '%s' skipped."),
					*Light->GetName(), *B.ParamName.ToString());
				return false;
			}
			// Reuse an existing MID so repeated resolves don't pile up new instances.
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Light->LightFunctionMaterial);
			if (!MID)
			{
				MID = UMaterialInstanceDynamic::Create(Light->LightFunctionMaterial, Light);
				Light->SetLightFunctionMaterial(MID);
			}
			Out.MID = MID;
			Out.MIDElementIndex = 0;
			return true;
		}

		// Volumetric cloud path: dynamic material on the cloud's Volume-domain material.
		if (UVolumetricCloudComponent* Cloud = FindComponentByTagOrName<UVolumetricCloudComponent>(Host, B.ComponentTag))
		{
			UMaterialInterface* CloudMat = Cloud->GetMaterial();
			if (!CloudMat)
			{
				CloudMat = Cloud->Material.LoadSynchronous();
			}
			if (!CloudMat)
			{
				UE_LOG(LogWeatherApplier, Warning, TEXT("Volumetric cloud %s has no material set; binding '%s' skipped."),
					*Cloud->GetName(), *B.ParamName.ToString());
				return false;
			}
			// Reuse an existing MID so repeated resolves don't pile up new instances.
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(CloudMat);
			if (!MID)
			{
				MID = UMaterialInstanceDynamic::Create(CloudMat, Cloud);
				Cloud->SetMaterial(MID);
			}
			Out.MID = MID;
			Out.MIDElementIndex = 0;
			return true;
		}

		UE_LOG(LogWeatherApplier, Warning, TEXT("No mesh, light or volumetric cloud component (tag=%s) on %s for material param '%s'"),
			*B.ComponentTag.ToString(), *Host->GetName(), *B.ParamName.ToString());
		return false;
	}
	case EWeatherParamTarget::MPCParameter:
	{
		UMaterialParameterCollection* MPC = Schema ? Schema->GetMPC() : nullptr;
		if (!MPC)
		{
			UE_LOG(LogWeatherApplier, Warning, TEXT("Schema has no MPC set; binding '%s' skipped."), *B.ParamName.ToString());
			return false;
		}
		Out.MPC = MPC;
		return true;
	}
	case EWeatherParamTarget::NiagaraParameter:
	{
		UNiagaraComponent* N = FindComponentByTagOrName<UNiagaraComponent>(Host, B.ComponentTag);
		if (!N)
		{
			UE_LOG(LogWeatherApplier, Warning, TEXT("No Niagara component (tag=%s) on %s for '%s'"),
				*B.ComponentTag.ToString(), *Host->GetName(), *B.ParamName.ToString());
			return false;
		}
		Out.Niagara = N;
		return true;
	}
	}
	return false;
}

void FParameterApplier::Apply(const TArray<FWeatherParamValue>& Values, UWorld* World) const
{
	const int32 N = FMath::Min(Values.Num(), Resolved.Num());
	for (int32 i = 0; i < N; ++i)
	{
		const FResolvedBinding& R = Resolved[i];
		if (!R.bValid) continue;
		WriteOne(R, Values[i], World);
	}
}

void FParameterApplier::ApplyBlended(const TArray<FWeatherParamValue>& A,
                                     const TArray<FWeatherParamValue>& B,
                                     float Alpha, UWorld* World) const
{
	const int32 N = FMath::Min3(A.Num(), B.Num(), Resolved.Num());
	FWeatherParamValue Tmp;
	for (int32 i = 0; i < N; ++i)
	{
		const FResolvedBinding& R = Resolved[i];
		if (!R.bValid) continue;
		Tmp = FWeatherParamValue::Lerp(A[i], B[i], Alpha);
		WriteOne(R, Tmp, World);
	}
}

void FParameterApplier::WriteOne(const FResolvedBinding& R, const FWeatherParamValue& V, UWorld* World) const
{
	switch (R.Target)
	{
	case EWeatherParamTarget::ActorProperty:
	{
		UObject* Owner = R.PropertyOwner.Get();
		if (!Owner || !R.Property) return;
		if (FFloatProperty* Fp = CastField<FFloatProperty>(R.Property))
		{
			Fp->SetPropertyValue_InContainer(Owner, V.FloatValue);
		}
		else if (FDoubleProperty* Dp = CastField<FDoubleProperty>(R.Property))
		{
			Dp->SetPropertyValue_InContainer(Owner, static_cast<double>(V.FloatValue));
		}
		else if (FStructProperty* Sp = CastField<FStructProperty>(R.Property))
		{
			if (Sp->Struct == TBaseStructure<FLinearColor>::Get())
			{
				*Sp->ContainerPtrToValuePtr<FLinearColor>(Owner) = V.LinearColorValue;
			}
			else if (Sp->Struct == TBaseStructure<FColor>::Get())
			{
				*Sp->ContainerPtrToValuePtr<FColor>(Owner) = V.ColorValue;
			}
		}
		// Raw UPROPERTY writes skip the setter that normally pushes CPU state to the
		// render / physics proxies (lights, fog, clouds, meshes all cache state there).
		// Poke the component so it rebuilds its render state. For anything that has a
		// real setter, prefer EWeatherParamTarget::ActorFunction — this is just a
		// safety net for properties without one.
		if (UActorComponent* Comp = Cast<UActorComponent>(Owner))
		{
			// Directional and sky lights maintain expensive GPU-side caches (CSM shadow
			// atlas, environment cubemap capture). MarkRenderStateDirty on them destroys
			// and re-creates the scene proxy each frame the applier writes, which stutters
			// hard when a weather crossfade is running. Skip them and rely on their setters
			// (SetIntensity / SetLightColor / RecaptureSky / ...) via ActorFunction bindings.
			const bool bSkipDirty = Comp->IsA<UDirectionalLightComponent>() || Comp->IsA<USkyLightComponent>();
			if (!bSkipDirty)
			{
				Comp->MarkRenderStateDirty();
				if (USceneComponent* Sc = Cast<USceneComponent>(Comp))
				{
					Sc->MarkRenderTransformDirty();
				}
			}
		}
		return;
	}
	case EWeatherParamTarget::ActorFunction:
	{
		UObject* Owner = R.FunctionOwner.Get();
		if (!Owner || !R.Function) return;
		// We support single-arg float or FLinearColor. Build a param block on the stack.
		uint8 Params[sizeof(FLinearColor)] = { 0 };
		if (R.Type == EWeatherParamType::Float)
		{
			*reinterpret_cast<float*>(Params) = V.FloatValue;
		}
		else if (R.Type == EWeatherParamType::LinearColor)
		{
			*reinterpret_cast<FLinearColor*>(Params) = V.LinearColorValue;
		}
		else if (R.Type == EWeatherParamType::Color)
		{
			// Best-effort: promote to LinearColor for the call.
			*reinterpret_cast<FLinearColor*>(Params) = FLinearColor(V.ColorValue);
		}
		Owner->ProcessEvent(R.Function, Params);
		return;
	}
	case EWeatherParamTarget::MaterialParameter:
	{
		UMaterialInstanceDynamic* MID = R.MID.Get();
		if (!MID) return;
		if (R.Type == EWeatherParamType::Float)
		{
			MID->SetScalarParameterValue(R.ParamName, V.FloatValue);
		}
		else
		{
			const FLinearColor L = (R.Type == EWeatherParamType::Color)
				? FLinearColor(V.ColorValue)
				: V.LinearColorValue;
			MID->SetVectorParameterValue(R.ParamName, L);
		}
		return;
	}
	case EWeatherParamTarget::MPCParameter:
	{
		UMaterialParameterCollection* MPC = R.MPC.Get();
		if (!MPC || !World) return;
		if (R.Type == EWeatherParamType::Float)
		{
			UKismetMaterialLibrary::SetScalarParameterValue(World, MPC, R.ParamName, V.FloatValue);
		}
		else
		{
			const FLinearColor L = (R.Type == EWeatherParamType::Color)
				? FLinearColor(V.ColorValue)
				: V.LinearColorValue;
			UKismetMaterialLibrary::SetVectorParameterValue(World, MPC, R.ParamName, L);
		}
		return;
	}
	case EWeatherParamTarget::NiagaraParameter:
	{
		UNiagaraComponent* N = R.Niagara.Get();
		if (!N) return;
		if (R.Type == EWeatherParamType::Float)
		{
			N->SetVariableFloat(R.ParamName, V.FloatValue);
		}
		else
		{
			const FLinearColor L = (R.Type == EWeatherParamType::Color)
				? FLinearColor(V.ColorValue)
				: V.LinearColorValue;
			N->SetVariableLinearColor(R.ParamName, L);
		}
		return;
	}
	}
}
