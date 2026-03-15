// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaterFluidTypes.h"
#include "WaterFluidConfigComponent.h"
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fluid Simulation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWaterFluidConfigComponent> FluidConfigComponent;
};
