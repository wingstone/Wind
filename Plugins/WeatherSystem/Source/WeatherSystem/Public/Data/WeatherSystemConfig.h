// Copyright TADemo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "WeatherSystemTypes.h"
#include "WeatherSystemConfig.generated.h"

/**
 * A single row in the transition DataTable, keyed logically by (From, Season).
 * Populate the row name however you like; RowName is only for editor convenience.
 * The controller iterates the DataTable and matches on the (From, Season) pair.
 */
USTRUCT(BlueprintType)
struct WEATHERSYSTEM_API FWeatherTransitionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (Categories = "Weather"))
	FGameplayTag From;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	EWeatherSeason Season = EWeatherSeason::Spring;

	/** Target weather -> unnormalized weight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (Categories = "Weather"))
	TMap<FGameplayTag, float> Weights;
};

/**
 * Global plugin configuration asset. Referenced by the subsystem / director.
 * Points at the transition table and holds default blend times.
 */
UCLASS(BlueprintType)
class WEATHERSYSTEM_API UWeatherSystemConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Master parameter schema for weathers. Applied as a defaults pass at the top of every
	 * subsystem tick BEFORE the active weather's sampled row is written, so channels a given
	 * weather doesn't touch always fall back to the schema default rather than lingering at
	 * whatever the previous weather / frame last wrote. Individual UWeatherDataAsset assets
	 * should reference this same schema.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	TObjectPtr<class UWeatherParamSchema> Schema;

	/** DataTable of FWeatherTransitionRow. Primary probability source for the controller. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather", meta = (RequiredAssetDataTags = "RowStructure=/Script/WeatherSystem.WeatherTransitionRow"))
	TSoftObjectPtr<UDataTable> TransitionTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	float DefaultWeatherBlendSeconds = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	float DefaultSeasonBlendSeconds = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	float DefaultAddonBlendSeconds = 3.0f;

	/** All weather / season / addon assets the subsystem should know about at boot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	TArray<TSoftObjectPtr<class UWeatherDataAsset>> Weathers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	TArray<TSoftObjectPtr<class USeasonDataAsset>> Seasons;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	TArray<TSoftObjectPtr<class UWeatherAddonDataAsset>> Addons;
};
