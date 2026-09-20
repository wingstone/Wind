// Copyright TADemo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "WeatherSystemTypes.h"
#include "WeatherFrame.h"
#include "SeasonDataAsset.generated.h"

class ASeasonActor;

/**
 * Season definition. Season frames are sampled by SeasonPhase (0..2).
 * WeatherWeights is a fallback probability table when the transition DataTable has
 * no row for (Current, Season).
 */
UCLASS(BlueprintType)
class WEATHERSYSTEM_API USeasonDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	EWeatherSeason Season = EWeatherSeason::Spring;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather",
		meta = (TitleProperty = "{DisplayName} ({ParamName})"))
	TArray<FWeatherParamBinding> Bindings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	TArray<FSeasonFrame> Frames;

	/** Fallback weather probability table (unnormalized). Consulted when the DataTable row is missing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (Categories = "Weather"))
	TMap<FGameplayTag, float> WeatherWeights;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	TSubclassOf<ASeasonActor> HostActorClass;

	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("Season"), GetFName());
	}

	void ResolveFrames();

private:
	void NormalizeFrames();
};
