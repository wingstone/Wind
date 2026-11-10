// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHIResources.h"
#include "Engine/Texture2D.h"
#include "Fluid2DFluidTypes.generated.h"

/**
 * Fluid solver type for 2D simulation.
 *
 * Solver types are not mutually exclusive: FFluid2DFluidConfig holds one independent layer per type,
 * so several of them can be enabled and simulated side by side.
 */
UENUM(BlueprintType)
enum class EFluidSolverType : uint8
{
	/** Shallow Water Equations - Fast, suitable for large-scale water bodies and height-based simulation */
	ShallowWater UMETA(DisplayName = "Shallow Water Equations"),
	
	/** 2D Incompressible Navier-Stokes - More accurate, suitable for detailed fluid dynamics with vorticity */
	NavierStokes UMETA(DisplayName = "2D Navier-Stokes"),

	/** Mask Accumulate - Used for special effects, such as murky water or other visual effects */
	MaskAccumulate UMETA(DisplayName = "Mask Accumulate"),

	/** Number of solver types. Keep last. */
	Count UMETA(Hidden)
};

/** Helpers for indexing the per-solver-type simulation layers. */
namespace Fluid2DSolverType
{
	/** Number of simulation layers, one per EFluidSolverType value. */
	constexpr int32 Count = static_cast<int32>(EFluidSolverType::Count);

	constexpr int32 ToIndex(EFluidSolverType SolverType)
	{
		return static_cast<int32>(SolverType);
	}

	constexpr EFluidSolverType FromIndex(int32 Index)
	{
		return static_cast<EFluidSolverType>(Index);
	}
}

UENUM(BlueprintType)
enum class ESourceShapeType : uint8
{
	Disk UMETA(DisplayName = "Disk Shape"),

	Ring UMETA(DisplayName = "Ring Shape"),

	Capsule UMETA(DisplayName = "Capsule Shape (swept disk)")
};

UENUM(BlueprintType)
enum class ESourceEmissionType : uint8
{
	Directional UMETA(DisplayName = "Directional Emission"),
	
	Omni UMETA(DisplayName = "Omni Emission"),

	Vortex UMETA(DisplayName = "Vortex Emission")
};

/**
 * Simulation domain and flow settings owned by a single solver layer.
 *
 * Every solver type derives from this, so coexisting layers can use different grids, world sizes,
 * time steps and flow noise - each layer is fully independent.
 */
USTRUCT(BlueprintType)
struct FFluid2DSolverConfigBase
{
	GENERATED_BODY()

	/** Whether this solver layer is simulated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation")
	bool bEnabled = true;

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

	/** Rest fluid level (cm) used by submerged tests */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation")
	float Fluid2DLevel = 5.0f;

	/** Noise texture used by Flow Volumes (intensity in R channel) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation|Flow")
	TObjectPtr<UTexture2D> FlowNoiseTexture = nullptr;

	/** Tiling scale of the flow noise texture over the simulation domain */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation|Flow", meta = (ClampMin = "0.01", UIMin = "0.01", UIMax = "10"))
	float FlowNoiseTiling = 1.0f;

	/** Minimum noise modulation applied to the flow speed when noise samples 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation|Flow", meta = (ClampMin = "0", UIMin = "0", UIMax = "2"))
	float FlowNoiseIntensityMin = 0.5f;

	/** Maximum noise modulation applied to the flow speed when noise samples 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation|Flow", meta = (ClampMin = "0", UIMin = "0", UIMax = "2"))
	float FlowNoiseIntensityMax = 1.0f;
};

/** Shallow water layer configuration */
USTRUCT(BlueprintType)
struct FSWFluidConfig : public FFluid2DSolverConfigBase
{
	GENERATED_BODY()

	/** Shallow water diffusion damping factor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float DiffusionDamping = 0.95f;

	/** Shallow water advection damping factor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float AdvectionDamping = 0.95f;

	/** Controls wave diffusion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float DiffusionAlpha = 0.25f;

	/** Controls wave diffusion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float DiffusionBeta = 0.95f;
};

/** Navier-Stokes layer configuration */
USTRUCT(BlueprintType)
struct FNSFluidConfig : public FFluid2DSolverConfigBase
{
	GENERATED_BODY()

	/** Viscosity coefficient */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "0", UIMin = "0", UIMax = "10000"))
	float Viscosity = 0.01f;

	/** Number of Jacobi iterations for the pressure solve */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "1", ClampMax = "100", UIMin = "1", UIMax = "50"))
	int32 NumPressureJacobiIterations = 20;

	/** Number of Jacobi iterations for the viscous diffusion solve */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "1", ClampMax = "100", UIMin = "1", UIMax = "50"))
	int32 NumDiffusionJacobiIterations = 20;

	/** Damping factor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float Damping = 1.0f;

	/** Vorticity confinement strength (epsilon). Higher values restore small-scale rotational detail lost to numerical dissipation. 0 = disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "0", UIMin = "0", UIMax = "10"))
	float VorticityConfinement = 0.0f;
};

/** Mask accumulate layer configuration */
USTRUCT(BlueprintType)
struct FMAFluidConfig : public FFluid2DSolverConfigBase
{
	GENERATED_BODY()

	/** Mask fade factor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float FadeFactor = 0.98f;
};

/**
 * Complete fluid configuration: one independent layer per solver type.
 *
 * Solver types are not mutually exclusive - enabling several simulates them side by side, each with
 * its own domain, fields and parameters.
 */
USTRUCT(BlueprintType)
struct FFluid2DFluidConfig
{
	GENERATED_BODY()

	FFluid2DFluidConfig()
	{
		// Shallow water is the default layer; the other two are opt-in.
		NavierStokes.bEnabled = false;
		MaskAccumulate.bEnabled = false;
	}

	/** Master switch for every solver layer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation")
	bool bEnableSimulation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation|Shallow Water")
	FSWFluidConfig ShallowWater;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation|Navier Stokes")
	FNSFluidConfig NavierStokes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation|Mask Accumulate")
	FMAFluidConfig MaskAccumulate;

	/** Configuration of the layer selected by InSolverType. */
	const FFluid2DSolverConfigBase& GetSolverConfig(EFluidSolverType InSolverType) const
	{
		switch (InSolverType)
		{
		case EFluidSolverType::NavierStokes: return NavierStokes;
		case EFluidSolverType::MaskAccumulate: return MaskAccumulate;
		default: return ShallowWater;
		}
	}

	FFluid2DSolverConfigBase& GetSolverConfig(EFluidSolverType InSolverType)
	{
		switch (InSolverType)
		{
		case EFluidSolverType::NavierStokes: return NavierStokes;
		case EFluidSolverType::MaskAccumulate: return MaskAccumulate;
		default: return ShallowWater;
		}
	}

	/** True when the layer of InSolverType is simulated. */
	bool IsSolverTypeEnabled(EFluidSolverType InSolverType) const
	{
		return bEnableSimulation && GetSolverConfig(InSolverType).bEnabled;
	}

	/** True when at least one layer is simulated. */
	bool HasEnabledSolverType() const
	{
		for (int32 Index = 0; Index < Fluid2DSolverType::Count; ++Index)
		{
			if (IsSolverTypeEnabled(Fluid2DSolverType::FromIndex(Index)))
			{
				return true;
			}
		}
		return false;
	}

	/** Enabled solver type with the lowest index, used for the default render output. Falls back to ShallowWater when none is enabled. */
	EFluidSolverType GetPrimarySolverType() const
	{
		for (int32 Index = 0; Index < Fluid2DSolverType::Count; ++Index)
		{
			const EFluidSolverType Candidate = Fluid2DSolverType::FromIndex(Index);
			if (IsSolverTypeEnabled(Candidate))
			{
				return Candidate;
			}
		}
		return EFluidSolverType::ShallowWater;
	}
};

/** Render-thread-safe snapshot of global flow data (produced by AFluid2DFlowVolume blending) */
struct FFluid2DGlobalFlowData
{
	FVector2D FlowDirection = FVector2D(1.0, 0.0);
	FTextureRHIRef FlowNoiseTextureRHI;
	float FlowNoiseIntensityMin = 0.0f;
	float FlowNoiseIntensityMax = 1.0f;
	float FlowNoiseTiling = 1.0f;
};

/** Player/object interaction data */
USTRUCT(BlueprintType)
struct FFluid2DInteractionData
{
	GENERATED_BODY()

	/** Interaction position in 2D */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FVector2D Position = FVector2D::ZeroVector;

	/** Previous-frame interaction position (used by Capsule shape to sweep between frames) */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FVector2D PrevPosition = FVector2D::ZeroVector;

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

	/** Solver layers this interaction applies to. Empty = every enabled layer. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TSet<EFluidSolverType> TargetSolverTypes;

	/** True when this interaction has to be applied to the given solver layer. */
	bool AppliesTo(EFluidSolverType InSolverType) const
	{
		return TargetSolverTypes.Num() == 0 || TargetSolverTypes.Contains(InSolverType);
	}
};
