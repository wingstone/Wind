// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Fluid2DGlobalFlowComponent.h"
#include "Fluid2DGlobalFlowActor.generated.h"

/**
 * Place in a level to drive a global noise-based flow field over the water simulation.
 * Provides ambient currents / river-like flow via UFluid2DGlobalFlowComponent.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Fluid2D Global Flow"))
class FLUID2DSYSTEMRUNTIME_API AFluid2DGlobalFlowActor : public AActor
{
	GENERATED_BODY()

public:
	AFluid2DGlobalFlowActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Global Flow", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFluid2DGlobalFlowComponent> GlobalFlowComponent;
};
