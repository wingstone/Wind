// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WindFieldSourceComponent.h"
#include "WindFieldDirectionalComponent.generated.h"

/**
 * Directional (global) wind source.
 * Affects the entire wind field uniformly along the component's forward direction.
 * Suitable for ambient/environmental wind.
 */
UCLASS(ClassGroup = (Wind), meta = (BlueprintSpawnableComponent, DisplayName = "Wind Field Directional Source"))
class WINDSYSTEMRUNTIME_API UWindFieldDirectionalComponent : public UWindFieldSourceComponent
{
	GENERATED_BODY()

public:
	UWindFieldDirectionalComponent();

	virtual EWindFieldSourceType GetWindType() const override { return EWindFieldSourceType::Directional; }
	virtual FGPUWindSourceData ToGPUData() const override;
};
