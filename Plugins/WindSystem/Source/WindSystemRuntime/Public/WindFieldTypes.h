// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WindFieldTypes.generated.h"

/** Wind source type enumeration */
UENUM(BlueprintType)
enum class EWindFieldSourceType : uint8
{
	Directional = 0 UMETA(DisplayName = "Directional"),
	Point       = 1 UMETA(DisplayName = "Point"),
	Vortex      = 2 UMETA(DisplayName = "Vortex"),
	Gust        = 3 UMETA(DisplayName = "Gust"),
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
	float     GustAmount;        //  4 bytes  offset 44
	float     GustFrequency;     //  4 bytes  offset 48
	float     NoiseStrength;     //  4 bytes  offset 52
	float     NoiseFrequency;    //  4 bytes  offset 56
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
	FIntVector Resolution = FIntVector(64, 16, 64);

	/** World-space extent of the wind field volume (cm), centered on the camera */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Field")
	FVector WorldExtent = FVector(6400.0, 1600.0, 6400.0);
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
