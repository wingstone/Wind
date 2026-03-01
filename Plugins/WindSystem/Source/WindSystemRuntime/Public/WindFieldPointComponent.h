// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WindFieldSourceComponent.h"
#include "WindFieldPointComponent.generated.h"

/**
 * Point (radial) wind source.
 * Pushes wind outward from the source position within a spherical radius.
 * Suitable for explosions, abilities, fans, etc.
 */
UCLASS(ClassGroup = (Wind), meta = (BlueprintSpawnableComponent, DisplayName = "Wind Field Point Source"))
class WINDSYSTEMRUNTIME_API UWindFieldPointComponent : public UWindFieldSourceComponent
{
	GENERATED_BODY()

public:
	UWindFieldPointComponent();

	virtual EWindFieldSourceType GetWindType() const override { return EWindFieldSourceType::Point; }
	virtual FGPUWindSourceData ToGPUData() const override;
};
