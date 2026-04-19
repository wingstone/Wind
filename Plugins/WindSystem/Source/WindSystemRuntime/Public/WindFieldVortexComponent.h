// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WindFieldSourceComponent.h"
#include "WindFieldVortexComponent.generated.h"

/**
 * Vortex (rotational) wind source.
 * Creates swirling wind around its up axis. Wind speed ramps up
 * from the center to InnerRadius, then fades toward Radius.
 * Suitable for tornadoes, cyclones, magical effects, etc.
 */
UCLASS(ClassGroup = (Wind), meta = (BlueprintSpawnableComponent, DisplayName = "Wind Field Vortex Source"))
class WINDSYSTEMRUNTIME_API UWindFieldVortexComponent : public UWindFieldSourceComponent
{
	GENERATED_BODY()

public:
	UWindFieldVortexComponent();

	/** Base wind strength (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Source")
	float Strength = 200.0f;

	/** Effect radius (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Source", meta = (ClampMin = "1"))
	float Radius = 500.0f;

	/** Inner dead-zone radius (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Source", meta = (ClampMin = "0"))
	float InnerRadius = 50.0f;

	/** Falloff exponent */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Source", meta = (ClampMin = "0"))
	float FalloffExponent = 1.5f;

	virtual EWindFieldSourceType GetWindType() const override { return EWindFieldSourceType::Vortex; }
	virtual FGPUWindSourceData ToGPUData() const override;
	virtual void DrawDebug(float Lifetime = 0.0f) const override;
};
