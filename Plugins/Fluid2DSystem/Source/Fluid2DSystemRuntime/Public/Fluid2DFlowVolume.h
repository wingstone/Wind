// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "Fluid2DFlowVolume.generated.h"

/**
 * Place in a level to drive a directional flow field over the water simulation
 * within a bounded region. Multiple volumes are resolved by priority with
 * BlendRadius controlling a soft inner falloff, similar to a PostProcessVolume.
 *
 * The flow noise texture and tiling are configured globally via UFluid2DSystemSettings
 * (or per-level via UFluid2DFluidConfigComponent). This volume only controls
 * direction / intensity within its region.
 */
UCLASS(Category = "Fluid2D", HideCategories = ("Rendering", "Collision", "Physics", "Networking", "Cooking", "Actor", "Input", "HLOD", "Replication", "Default"), meta = (DisplayName = "Fluid2D Flow Volume"))
class FLUID2DSYSTEMRUNTIME_API AFluid2DFlowVolume : public AVolume
{
	GENERATED_BODY()

public:
	AFluid2DFlowVolume();

	/** Whether this volume is enabled or not. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid2D Flow Volume")
	uint32 bEnabled : 1;

	/** Whether this volume covers the whole world, or just the area inside its bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid2D Flow Volume", meta = (DisplayName = "Infinite Extent (Unbound)"))
	uint32 bUnbound : 1;

	/** Higher priority volumes win against lower priority ones when they overlap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid2D Flow Volume")
	float Priority;

	/** World space distance inside the volume over which the flow blends from full to zero at the boundary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid2D Flow Volume", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "6000.0"))
	float BlendRadius;

	/** Flow direction in world space (XY only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid2D Flow Volume")
	FVector2D FlowDirection;

	/** Base flow speed (cm/s) applied inside this volume. Modulated by the global flow noise (min/max in FluidConfig). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid2D Flow Volume", meta = (ClampMin = "0", UIMin = "0", UIMax = "1000"))
	float Speed;

	//~ Begin AActor Interface
	virtual void PostUnregisterAllComponents() override;
	virtual void PostRegisterAllComponents() override;
	//~ End AActor Interface
};
