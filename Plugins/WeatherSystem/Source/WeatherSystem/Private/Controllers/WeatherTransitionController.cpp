// Copyright TADemo. All Rights Reserved.

#include "Controllers/WeatherTransitionController.h"

#include "Data/SeasonDataAsset.h"
#include "Data/WeatherSystemConfig.h"
#include "Engine/DataTable.h"

namespace
{
	FGameplayTag RouletteFromMap(const TMap<FGameplayTag, float>& Weights, FRandomStream& Stream)
	{
		float Total = 0.0f;
		for (const auto& It : Weights) { Total += FMath::Max(0.0f, It.Value); }
		if (Total <= 0.0f) return FGameplayTag();

		float Pick = Stream.FRandRange(0.0f, Total);
		for (const auto& It : Weights)
		{
			Pick -= FMath::Max(0.0f, It.Value);
			if (Pick <= 0.0f) return It.Key;
		}
		// Numerical fallthrough
		for (const auto& It : Weights)
		{
			if (It.Value > 0.0f) return It.Key;
		}
		return FGameplayTag();
	}
}

bool UWeatherTransitionController::PickNext(FGameplayTag Current,
                                            EWeatherSeason Season,
                                            const UDataTable* TransitionTable,
                                            const USeasonDataAsset* SeasonAsset,
                                            FRandomStream& Stream,
                                            FGameplayTag& OutNext) const
{
	// 1) Look for a (From, Season) row.
	if (TransitionTable)
	{
		for (const auto& RowIt : TransitionTable->GetRowMap())
		{
			const FWeatherTransitionRow* Row = reinterpret_cast<const FWeatherTransitionRow*>(RowIt.Value);
			if (!Row) continue;
			if (Row->Season != Season) continue;
			if (Row->From != Current) continue;

			const FGameplayTag Picked = RouletteFromMap(Row->Weights, Stream);
			if (Picked.IsValid())
			{
				OutNext = Picked;
				return true;
			}
			break;
		}
	}

	// 2) Fallback: season's own weight map.
	if (SeasonAsset)
	{
		const FGameplayTag Picked = RouletteFromMap(SeasonAsset->WeatherWeights, Stream);
		if (Picked.IsValid())
		{
			OutNext = Picked;
			return true;
		}
	}

	return false;
}
