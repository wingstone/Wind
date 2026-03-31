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

UENUM(BlueprintType)
enum class ESourceShapeType : uint8
{
	Disk UMETA(DisplayName = "Disk Shape"),
	
	Ring UMETA(DisplayName = "Ring Shape")
};

UENUM(BlueprintType)
enum class ESourceEmissionType : uint8
{
	Directional UMETA(DisplayName = "Directional Emission"),
	
	Omni UMETA(DisplayName = "Omni Emission"),

	Vortex UMETA(DisplayName = "Vortex Emission")
};

/** 2D fluid simulation grid configuration */
USTRUCT(BlueprintType)
struct FWaterFluidConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation")
	bool bEnableSimulation = true;

	/** Solver type - choose between Shallow Water or Navier-Stokes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation")
	EFluidSolverType SolverType = EFluidSolverType::ShallowWater;

	/** Grid resolution (width = height) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "32", ClampMax = "1024"))
	int32 GridSize = 256;

	/** Physical size of simulation area (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "100"))
	float WorldSize = 10000.0f;

	/** Simulation time step (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "0.0", ClampMax = "0.1"))
	float TimeStep = 0.016f;

	/** Simulation quality (substeps per frame) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "1", ClampMax = "10"))
	int32 SimulationSubsteps = 1;
	
	/** Initial water level (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation")
	float WaterLevel = 5.0f;

	/** Viscosity coefficient */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "0", UIMin = "0", UIMax = "10000"))
	float NS_Viscosity = 0.01f;

	/** Number of Jacobi iterations for pressure solve */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "1", ClampMax = "100", UIMin = "1", UIMax = "50"))
	int32 NS_NumPressureJacobiIterations = 20;

	/** Number of Jacobi iterations for viscous diffusion solve */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "1", ClampMax = "100", UIMin = "1", UIMax = "50"))
	int32 NS_NumDiffusionJacobiIterations = 20;
	
	/** Navier-Stokes damping factor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float NS_Damping = 1.0f;

	/** Vorticity confinement strength (epsilon). Higher values restore small-scale rotational detail lost to numerical dissipation. 0 = disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "0", UIMin = "0", UIMax = "10"))
	float NS_VorticityConfinement = 0.0f;

	/** Shallow water diffusion damping factor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float SW_DiffusionDamping = 0.95f;
	
	/** Only used for shallow water solver, controls wave diffusion */ 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float SW_DiffusionAlpha = 0.25f;

	/** Only used for shallow water solver, controls wave diffusion */ 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float SW_DiffusionBeta = 0.95f;
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
	FVector2D ForceDirection = FVector2D::ZeroVector;

	/** Interaction radius or ring radius and width */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FVector2D RadiusParameter = FVector2D(100.0f, 0.0f);

	/** Interaction strength paramter: direction, omni, vortex */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FVector StrengthParameter = FVector(1.0f, 0.0f, 0.0f);

	/** Interaction Power falloff， Only used for disk shape */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float PowerFalloff = 0.0f;

	/** Interaction Gaussian falloff， Only used for disk shape */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float HeightIntensity = 1.0f;

	/** Interaction shape type */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	ESourceShapeType ShapeType = ESourceShapeType::Disk;

	/** Interaction emission type */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	ESourceEmissionType EmissionType = ESourceEmissionType::Directional;
};
