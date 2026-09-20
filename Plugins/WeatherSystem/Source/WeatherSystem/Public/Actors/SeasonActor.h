// Copyright TADemo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SeasonActor.generated.h"

/**
 * Shared host actor for season parameters. Season bindings can target multiple named
 * components (trees / water body / etc.) via ComponentTag on each FWeatherParamBinding.
 */
UCLASS(BlueprintType, Blueprintable)
class WEATHERSYSTEM_API ASeasonActor : public AActor
{
	GENERATED_BODY()

public:
	ASeasonActor();

	//~ AActor
	virtual void PostRegisterAllComponents() override;
	virtual void PostUnregisterAllComponents() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Weather")
	void OnSeasonAssetBound(const class USeasonDataAsset* NewAsset);
};
