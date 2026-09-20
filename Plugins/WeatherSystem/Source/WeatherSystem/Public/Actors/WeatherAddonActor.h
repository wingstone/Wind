// Copyright TADemo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "WeatherAddonActor.generated.h"

class UWeatherAddonDataAsset;

/**
 * Independent host actor for a single weather addon (rainbow / aurora / lightning / ...).
 * One instance per active addon; owns its own applier state via the subsystem.
 *
 * The current sampled Weight is pushed via SetAddonWeight so BPs / content can gate their
 * own visual updates on it in addition to the applier's per-binding writes.
 */
UCLASS(BlueprintType, Blueprintable)
class WEATHERSYSTEM_API AWeatherAddonActor : public AActor
{
	GENERATED_BODY()

public:
	AWeatherAddonActor();

	//~ AActor
	/** Self-register with the subsystem. Designers place one actor per addon and set AddonId. */
	virtual void PostRegisterAllComponents() override;
	virtual void PostUnregisterAllComponents() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End AActor

	UPROPERTY(BlueprintReadOnly, Category = "Weather")
	FGameplayTag AddonId;

	UPROPERTY(BlueprintReadOnly, Category = "Weather")
	TObjectPtr<UWeatherAddonDataAsset> AddonAsset;

	/** Latest sampled master weight in [0,1]. Set by the subsystem every tick. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather")
	float CurrentWeight = 0.0f;

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetAddonWeight(float NewWeight);

	UFUNCTION(BlueprintImplementableEvent, Category = "Weather")
	void OnWeightChanged(float NewWeight);

	UFUNCTION(BlueprintImplementableEvent, Category = "Weather")
	void OnAddonAssetBound(const UWeatherAddonDataAsset* NewAsset);
};
