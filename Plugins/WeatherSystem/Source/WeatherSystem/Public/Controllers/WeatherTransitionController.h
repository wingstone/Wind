// Copyright TADemo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "WeatherSystemTypes.h"
#include "WeatherTransitionController.generated.h"

class UDataTable;
class USeasonDataAsset;

/**
 * Picks the next weather from (Current, Season) using a DataTable of FWeatherTransitionRow.
 * Falls back to USeasonDataAsset::WeatherWeights when the DataTable has no matching row.
 * Deterministic against an external FRandomStream — pass one in for reproducible sequences.
 */
UCLASS(BlueprintType)
class WEATHERSYSTEM_API UWeatherTransitionController : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * @param Current           current weather tag (may be invalid, e.g. on boot)
	 * @param Season            current season
	 * @param TransitionTable   DataTable of FWeatherTransitionRow — may be null
	 * @param SeasonAsset       fallback probability source — may be null
	 * @param Stream            random source; state is advanced
	 * @param OutNext           picked weather (unchanged if nothing pickable)
	 * @return true if a candidate was found
	 */
	bool PickNext(FGameplayTag Current,
	              EWeatherSeason Season,
	              const UDataTable* TransitionTable,
	              const USeasonDataAsset* SeasonAsset,
	              FRandomStream& Stream,
	              FGameplayTag& OutNext) const;
};
