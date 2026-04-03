// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Fluid2DFluidTypes.h"
#include "Fluid2DFluidConfigComponent.h"
#include "Fluid2DFluidConfigActor.generated.h"

/**
 * Place in a level to override the default water fluid configuration
 * from Project Settings (UFluid2DSystemSettings) for this specific level.
 *
 * Works in both editor and runtime:
 * - Editor: configuration applies immediately when placed or when properties change
 * - Runtime: configuration applies during OnConstruction
 * - Deletion: reverts subsystem back to Project Settings defaults
 */
UCLASS(BlueprintType, meta = (DisplayName = "Fluid2D Fluid Config"))
class FLUID2DSYSTEMRUNTIME_API AFluid2DFluidConfigActor : public AActor
{
	GENERATED_BODY()

public:
	AFluid2DFluidConfigActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fluid Simulation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFluid2DFluidConfigComponent> FluidConfigComponent;
};
