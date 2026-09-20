// Copyright TADemo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "WeatherSystemTypes.h"
#include "WeatherFrame.h"
#include "WeatherAddonDataAsset.generated.h"

class AWeatherAddonActor;
class UWeatherParamSchema;

/**
 * Definition of a weather-addon layer (rainbow, aurora, lightning, ...).
 * Each addon spawns its own actor and drives its own bindings — no cross-addon mixing.
 */
UCLASS(BlueprintType)
class WEATHERSYSTEM_API UWeatherAddonDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather", meta = (Categories = "Addon"))
	FGameplayTag AddonId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	FText DisplayName;

	/** Each addon gets its own independent Actor instance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	TSubclassOf<AWeatherAddonActor> ActorClass;

	/** Shared column definitions. Every FAddonFrame's Values.Num() must equal Schema->Bindings.Num(). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	TObjectPtr<UWeatherParamSchema> Schema;

	/** Applied once when this addon becomes active. Keyed by ParamName; matched against Schema. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	TMap<FName, FWeatherParamValue> Constants;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	TArray<FAddonFrame> Frames;

	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("WeatherAddon"), GetFName());
	}

	void BuildConstantValues(TArray<FWeatherParamValue>& Out) const;

	void ResolveFrames();

private:
	void NormalizeFrames();
};
