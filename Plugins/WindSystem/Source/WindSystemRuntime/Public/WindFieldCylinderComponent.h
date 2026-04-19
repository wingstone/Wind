// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WindFieldSourceComponent.h"
#include "WindFieldCylinderComponent.generated.h"

/**
 * Cylinder / Cone wind source.
 * Pushes wind along the component's forward axis within a cylindrical or
 * conical volume.  When EndRadius == Radius it is a perfect cylinder;
 * when EndRadius == 0 it degenerates to a cone.
 *
 * The volume is centered on the component location.
 * - Base end (−Forward) has radius = Radius
 * - Top  end (+Forward) has radius = EndRadius
 */
UCLASS(ClassGroup = (Wind), meta = (BlueprintSpawnableComponent, DisplayName = "Wind Field Cylinder Source"))
class WINDSYSTEMRUNTIME_API UWindFieldCylinderComponent : public UWindFieldSourceComponent
{
	GENERATED_BODY()

public:
	UWindFieldCylinderComponent();

	/** Base wind strength (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Source")
	float Strength = 400.0f;

	/** Base radius at −Forward end (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Source", meta = (ClampMin = "1"))
	float Radius = 300.0f;

	/** Inner radius with full strength (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Source", meta = (ClampMin = "0"))
	float InnerRadius = 0.0f;

	/** Falloff exponent */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Source", meta = (ClampMin = "0"))
	float FalloffExponent = 1.5f;

	/** Half-height of the cylinder along the forward axis (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Source", meta = (ClampMin = "1"))
	float HalfHeight = 500.0f;

	/** Radius at the top end (+Forward). Set to 0 for a cone, equal to Radius for a cylinder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Source", meta = (ClampMin = "0"))
	float EndRadius = 0.0f;

	virtual EWindFieldSourceType GetWindType() const override { return EWindFieldSourceType::Cylinder; }
	virtual FGPUWindSourceData ToGPUData() const override;
	virtual void DrawDebug(float Lifetime = 0.0f) const override;
};
