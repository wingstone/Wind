// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/UnrealType.h"
#include "Fluid2DFluidTypes.h"
#include "Fluid2DSystemSettings.generated.h"

/**
 * Global water system settings accessible via Project Settings → Game → Fluid2D System.
 * Provides default FluidConfig values that can be overridden per-level
 * by placing an AFluid2DFluidConfigActor in the level.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Fluid2D System"))
class FLUID2DSYSTEMRUNTIME_API UFluid2DSystemSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Default fluid simulation configuration */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Fluid Simulation")
	FFluid2DFluidConfig DefaultFluidConfig;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	static const UFluid2DSystemSettings* Get() { return GetDefault<UFluid2DSystemSettings>(); }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnUpdateSettings, const UFluid2DSystemSettings* /*Settings*/, EPropertyChangeType::Type /*ChangeType*/);
	static FOnUpdateSettings OnSettingsChange;
#endif
};
