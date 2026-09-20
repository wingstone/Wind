// Copyright TADemo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "WeatherSystemTypes.h"

class AActor;
class UMaterialInstanceDynamic;
class UMaterialParameterCollection;
class UNiagaraComponent;
class UWeatherParamSchema;

/**
 * One binding, pre-resolved: property pointer / MID / MPC / Niagara handle cached so the
 * per-frame Apply() path is index-parallel writes with no reflection lookups.
 */
struct FResolvedBinding
{
	EWeatherParamTarget Target = EWeatherParamTarget::MaterialParameter;
	EWeatherParamType   Type   = EWeatherParamType::Float;
	FName               ParamName;

	// Actor property write
	FProperty*                                    Property = nullptr;
	TWeakObjectPtr<UObject>                       PropertyOwner;

	// Function call
	UFunction*                                    Function = nullptr;
	TWeakObjectPtr<UObject>                       FunctionOwner;

	// Material instance (dynamic) on a mesh component (element MIDElementIndex) or
	// on a light component's light function material.
	TWeakObjectPtr<UMaterialInstanceDynamic>      MID;
	int32                                         MIDElementIndex = 0;

	// MPC (world-scope global)
	TWeakObjectPtr<UMaterialParameterCollection>  MPC;

	// Niagara user param
	TWeakObjectPtr<UNiagaraComponent>             Niagara;

	bool bValid = false;
};

/**
 * Owns the resolved-binding cache for one host actor + one binding list.
 * Rebuild by calling ResolveBindings whenever the host actor or the source binding list changes.
 * Apply() is the hot path — pure index loops.
 */
class WEATHERSYSTEM_API FParameterApplier
{
public:
	/**
	 * Rebuild resolved cache against a schema on a given host actor. Passing null schema clears the cache.
	 * MPC references are looked up through the schema's MPCs table by FWeatherParamBinding::MPCIndex.
	 */
	void ResolveBindings(AActor* Host, const UWeatherParamSchema* Schema);

	/** Overload: resolve without a schema (MPC bindings will fail with a warning). */
	void ResolveBindingsRaw(AActor* Host, const TArray<FWeatherParamBinding>& InBindings);

	/** Write a single Values array (already blended) through the resolved cache. */
	void Apply(const TArray<FWeatherParamValue>& Values, UWorld* World) const;

	/** Blend A -> B by Alpha and apply. Avoids the caller allocating a temp array. */
	void ApplyBlended(const TArray<FWeatherParamValue>& A,
	                  const TArray<FWeatherParamValue>& B,
	                  float Alpha, UWorld* World) const;

	void Reset() { Resolved.Reset(); }

	int32 Num() const { return Resolved.Num(); }

private:
	TArray<FResolvedBinding> Resolved;

	/** Populate one resolved entry from one binding; returns false if unrecoverable. */
	bool ResolveOne(AActor* Host, const FWeatherParamBinding& Binding, const UWeatherParamSchema* Schema, FResolvedBinding& Out) const;

	void WriteOne(const FResolvedBinding& R, const FWeatherParamValue& V, UWorld* World) const;
};
