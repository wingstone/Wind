// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaterFluidTypes.h"
#include "WaterFluidConfigActor.generated.h"

/**
 * Place in a level to override the default water fluid configuration
 * from Project Settings (UWaterSystemSettings) for this specific level.
 *
 * Works in both editor and runtime:
 * - Editor: configuration applies immediately when placed or when properties change
 * - Runtime: configuration applies during OnConstruction
 * - Deletion: reverts subsystem back to Project Settings defaults
 */
UCLASS(BlueprintType, meta = (DisplayName = "Water Fluid Config"))
class WATERSYSTEMRUNTIME_API AWaterFluidConfigActor : public AActor
{
	GENERATED_BODY()

public:
	AWaterFluidConfigActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation")
	FWaterFluidConfig FluidConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation")
	bool bOverrideEnableSimulation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation",
		meta = (EditCondition = "bOverrideEnableSimulation"))
	bool bEnableSimulation = true;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Destroyed() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void ApplyConfigToSubsystem();
	void RevertConfigToDefaults();
};
