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

	/** Base wind strength (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Source")
	float Strength = 300.0f;

	/** Effect radius (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Source", meta = (ClampMin = "1"))
	float Radius = 500.0f;

	/** Inner radius with full strength (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Source", meta = (ClampMin = "0"))
	float InnerRadius = 50.0f;

	/** Falloff exponent (1 = linear, 2 = quadratic, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Source", meta = (ClampMin = "0"))
	float FalloffExponent = 2.0f;

	virtual EWindFieldSourceType GetWindType() const override { return EWindFieldSourceType::Point; }
	virtual FGPUWindSourceData ToGPUData() const override;
	virtual void DrawDebug(float Lifetime = 0.0f) const override;
};
