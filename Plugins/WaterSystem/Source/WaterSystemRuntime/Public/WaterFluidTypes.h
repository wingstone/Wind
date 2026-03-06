// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WaterFluidTypes.generated.h"

/**
 * Fluid solver type for 2D simulation
 */
UENUM(BlueprintType)
enum class EFluidSolverType : uint8
{
	/** Shallow Water Equations - Fast, suitable for large-scale water bodies and height-based simulation */
	ShallowWater UMETA(DisplayName = "Shallow Water Equations"),
	
	/** 2D Incompressible Navier-Stokes - More accurate, suitable for detailed fluid dynamics with vorticity */
	NavierStokes UMETA(DisplayName = "2D Navier-Stokes")
};

/** 2D fluid simulation grid configuration */
USTRUCT(BlueprintType)
struct FWaterFluidConfig
{
	GENERATED_BODY()

	/** Solver type - choose between Shallow Water or Navier-Stokes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation")
	EFluidSolverType SolverType = EFluidSolverType::ShallowWater;

	/** Grid resolution (width = height) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "32", ClampMax = "1024"))
	int32 GridSize = 256;

	/** Physical size of simulation area (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "1000"))
	float WorldSize = 10000.0f;

	/** Fluid density (g/cm³) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "0.01"))
	float Density = 1.0f;

	/** Viscosity coefficient */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "0"))
	float Viscosity = 0.01f;

	/** Wave damping factor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "0", ClampMax = "1"))
	float Damping = 0.02f;

	/** Gravity strength (cm/s²) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "0"))
	float Gravity = 980.0f;

	/** Surface tension coefficient */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "0"))
	float SurfaceTension = 0.05f;

	/** Simulation time step (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "0.001", ClampMax = "0.1"))
	float TimeStep = 0.016f;
};

/** GPU-uploadable water disturbance data */
struct FGPUWaterDisturbance
{
	FVector2D Position;      // XY position in 2D field
	float Strength;          // Disturbance strength
	float Radius;            // Effect radius
	float Falloff;           // Falloff exponent
	float Duration;          // Duration of disturbance
	float ElapsedTime;       // Time since start
	float Padding;           // Align to 16 bytes

	FGPUWaterDisturbance()
		: Position(ForceInitToZero)
		, Strength(0.0f)
		, Radius(0.0f)
		, Falloff(2.0f)
		, Duration(1.0f)
		, ElapsedTime(0.0f)
		, Padding(0.0f)
	{}
};

/** Player/object interaction data */
USTRUCT(BlueprintType)
struct FWaterInteractionData
{
	GENERATED_BODY()

	/** Interaction position in 2D */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FVector2D Position = FVector2D::ZeroVector;

	/** Interaction force/velocity */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FVector2D Force = FVector2D::ZeroVector;

	/** Interaction radius */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float Radius = 100.0f;

	/** Interaction strength multiplier */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float Strength = 1.0f;
};
