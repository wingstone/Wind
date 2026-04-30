// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "WindFieldTypes.generated.h"

/** Wind source type enumeration */
UENUM(BlueprintType)
enum class EWindFieldSourceType : uint8
{
	Directional = 0 UMETA(DisplayName = "Directional"),
	Point       = 1 UMETA(DisplayName = "Point"),
	Vortex      = 2 UMETA(DisplayName = "Vortex"),
	Cylinder    = 3 UMETA(DisplayName = "Cylinder"),
	CapsuleInteractive = 4 UMETA(DisplayName = "Capsule Interactive"),
};

/** Diffusion solver method */
UENUM(BlueprintType)
enum class EWindDiffusionMethod : uint8
{
	/** Explicit forward Euler finite difference. Fast but requires small Viscosity for stability. */
	FiniteDifference = 0 UMETA(DisplayName = "Finite Difference (Explicit)"),
	/** Implicit Jacobi iteration. Unconditionally stable, smoother results. */
	Jacobi           = 1 UMETA(DisplayName = "Jacobi (Implicit)"),
};

/**
 * GPU-compatible wind source data layout.
 * Must match the HLSL FGPUWindSource struct exactly (64 bytes, tightly packed).
 */
struct FGPUWindSourceData
{
	FVector3f Position;          // 12 bytes  offset 0
	float     Strength;          //  4 bytes  offset 12
	FVector3f Direction;         // 12 bytes  offset 16
	float     Radius;            //  4 bytes  offset 28
	float     InnerRadius;       //  4 bytes  offset 32
	float     FalloffExponent;   //  4 bytes  offset 36
	uint32    WindType;          //  4 bytes  offset 40
	float     HalfHeight;        //  4 bytes  offset 44  (Cylinder/Cone)
	float     EndRadius;         //  4 bytes  offset 48  (Cylinder/Cone top radius)
	float     Padding3;          //  4 bytes  offset 52  (CapsuleInteractive: AngularVelocity.Y)
	float     Padding4;          //  4 bytes  offset 56  (CapsuleInteractive: AngularVelocity.Z)
	float     Padding;           //  4 bytes  offset 60
	// Total: 64 bytes
};

static_assert(sizeof(FGPUWindSourceData) == 64, "FGPUWindSourceData must be 64 bytes to match HLSL layout");

/** Wind field 3D texture configuration */
USTRUCT(BlueprintType)
struct WINDSYSTEMRUNTIME_API FWindFieldConfig
{
	GENERATED_BODY()

	/** Resolution of the 3D wind field texture (texels per axis) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Field")
	FIntVector Resolution = FIntVector(64, 64, 64);

	/** World-space extent of the wind field volume (cm), centered on the camera */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Field")
	FVector WorldExtent = FVector(6400.0, 1600.0, 6400.0);

	/** Viscosity coefficient for diffusion (higher = smoother, more viscous wind).
	 *  Controls the strength of the Laplacian smoothing term. Keep below 0.15 for stability. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Field", meta = (ClampMin = "0", UIMin = "0", UIMax = "100000"))
	float Viscosity = 0.05f;

	/** Dissipation factor per frame (lower = faster energy decay, 1.0 = no decay).
	 *  At steady state the wind velocity equals the source's contribution. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Field", meta = (ClampMin = "0.8", ClampMax = "1.0"))
	float Dissipation = 0.98f;

	/** Simulation time step (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Field", meta = (ClampMin = "0.0", ClampMax = "0.1"))
	float TimeStep = 0.016f;

	/** Diffusion solver method.
	 *  Finite Difference: explicit forward Euler, fast but needs small Viscosity (< 0.1).
	 *  Jacobi: implicit solver, unconditionally stable, smoother results. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Field")
	EWindDiffusionMethod DiffusionMethod = EWindDiffusionMethod::Jacobi;

	/** Number of Jacobi iterations per frame (only used when DiffusionMethod == Jacobi).
	 *  More iterations = better convergence / smoother result, but higher GPU cost. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Field", meta = (ClampMin = "1", ClampMax = "30", EditCondition = "DiffusionMethod == EWindDiffusionMethod::Jacobi"))
	int32 JacobiIterations = 4;
};

/** Result of sampling the wind field at a world position */
USTRUCT(BlueprintType)
struct WINDSYSTEMRUNTIME_API FWindSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Wind")
	FVector WindVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Wind")
	float Turbulence = 0.0f;
};

/**
 * Render-thread data for the global directional wind.
 * Transported from game thread via ENQUEUE_RENDER_COMMAND.
 * Contains wind direction/strength, curl noise, and optional noise texture.
 * The directional wind is static — it does not participate in fluid iteration.
 */
struct FWindDirectionalData
{
	/** Whether directional wind is enabled */
	bool bEnabled = false;

	// --- Wind parameters ---
	FVector3f WindDirection = FVector3f(1.0f, 0.0f, 0.0f);
	float Strength = 0.0f;
	
	// --- Noise texture modulation ---
	FTextureRHIRef NoiseTextureRHI = nullptr;
	float NoiseStrength = 0.0f;
	float Tiling = 1.0f;
	float IntensityMin = 0.2f;
	float IntensityMax = 1.0f;
	float ScrollSpeed = 50.0f;

	bool IsValid() const { return bEnabled; }
	bool HasNoiseTexture() const { return NoiseTextureRHI != nullptr; }
};
