// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "WaterFluidTypes.h"
#include "WaterSystemSettings.generated.h"

/**
 * Global water system settings accessible via Project Settings → Game → Water System.
 * Provides default FluidConfig values that can be overridden per-level
 * by placing an AWaterFluidConfigActor in the level.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Water System"))
class WATERSYSTEMRUNTIME_API UWaterSystemSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Default fluid simulation configuration */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Fluid Simulation")
	FWaterFluidConfig DefaultFluidConfig;

	/** Enable 2D fluid simulation by default */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Fluid Simulation")
	bool bEnableSimulation = true;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	static const UWaterSystemSettings* Get() { return GetDefault<UWaterSystemSettings>(); }
};
