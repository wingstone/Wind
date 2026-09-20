// Copyright TADemo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Materials/MaterialParameterCollection.h"
#include "WeatherSystemTypes.h"
#include "WeatherParamSchema.generated.h"

/**
 * A reusable "column" definition for weather / addon / local-volume frames.
 * Author once and reference from many UWeatherDataAsset / UWeatherAddonDataAsset:
 * every frame across those assets ends up using the exact same channel order and
 * types, which keeps SoA sampling and reflection resolution trivially aligned.
 *
 * Season assets keep their own inline bindings — season frames typically drive an
 * entirely different set of targets (trees, water bodies) and aren't worth cross-
 * pollinating with weather.
 */
UCLASS(BlueprintType)
class WEATHERSYSTEM_API UWeatherParamSchema : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Ordered channel list. Order defines the row layout of every FWeatherFrame using this schema. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather",
		meta = (TitleProperty = "{DisplayName} ({ParamName})"))
	TArray<FWeatherParamBinding> Bindings;

	/**
	 * The single MPC that every MPCParameter binding on this schema writes into.
	 * By design, all MPC-targeted parameters in one schema share one MPC asset — keeps
	 * the runtime write path branch-free and the editor UX simple.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	TSoftObjectPtr<UMaterialParameterCollection> MPC;

	UMaterialParameterCollection* GetMPC() const { return MPC.LoadSynchronous(); }

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("WeatherParamSchema"), GetFName());
	}
};
