// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindFieldTypes.h"
#include "WindFieldConfigComponent.h"
#include "WindFieldConfigActor.generated.h"

/**
 * Place in a level to override the default wind field configuration
 * from Project Settings (UWindSystemSettings) for this specific level.
 *
 * Works in both editor and runtime:
 * - Editor: configuration applies immediately when placed or when properties change
 * - Runtime: configuration applies during BeginPlay
 * - Deletion: reverts subsystem back to Project Settings defaults
 */
UCLASS(BlueprintType, meta = (DisplayName = "Wind Field Config"))
class WINDSYSTEMRUNTIME_API AWindFieldConfigActor : public AActor
{
	GENERATED_BODY()

public:
	AWindFieldConfigActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wind Field", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWindFieldConfigComponent> WindFieldConfigComponent;
};
