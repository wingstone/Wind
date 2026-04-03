// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaterGlobalFlowComponent.h"
#include "WaterGlobalFlowActor.generated.h"

/**
 * Place in a level to drive a global noise-based flow field over the water simulation.
 * Provides ambient currents / river-like flow via UWaterGlobalFlowComponent.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Water Global Flow"))
class WATERSYSTEMRUNTIME_API AWaterGlobalFlowActor : public AActor
{
	GENERATED_BODY()

public:
	AWaterGlobalFlowActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Global Flow", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWaterGlobalFlowComponent> GlobalFlowComponent;
};
