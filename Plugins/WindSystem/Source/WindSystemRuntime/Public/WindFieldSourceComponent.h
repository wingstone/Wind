// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "WindFieldTypes.h"
#include "WindFieldSourceComponent.generated.h"

class UWindSubsystem;

/**
 * Base class for all wind field source components.
 * Provides common wind parameters and auto-registers with UWindSubsystem.
 */
UCLASS(Abstract, ClassGroup = (Wind), meta = (BlueprintSpawnableComponent))
class WINDSYSTEMRUNTIME_API UWindFieldSourceComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UWindFieldSourceComponent();

	// --- Wind parameters ---

	/** Base wind strength (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Source")
	float Strength = 100.0f;

	/** Effect radius (cm). 0 = infinite (directional wind) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Source", meta = (ClampMin = "0"))
	float Radius = 1000.0f;

	/** Inner radius with no falloff (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Source", meta = (ClampMin = "0"))
	float InnerRadius = 0.0f;

	/** Falloff exponent (1 = linear, 2 = quadratic, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Source", meta = (ClampMin = "0.1"))
	float FalloffExponent = 1.0f;

	/** Gust variation amount (0 = no gusting, 1 = full variation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Source|Gusting", meta = (ClampMin = "0", ClampMax = "2"))
	float GustAmount = 0.2f;

	/** Gust oscillation frequency (Hz) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Source|Gusting", meta = (ClampMin = "0.01"))
	float GustFrequency = 1.0f;

	/** Curl noise turbulence strength */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Source|Turbulence", meta = (ClampMin = "0"))
	float NoiseStrength = 0.3f;

	/** Curl noise frequency (spatial scale) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Source|Turbulence", meta = (ClampMin = "0.01"))
	float NoiseFrequency = 1.0f;

	// --- Interface ---

	/** Convert component parameters to GPU-uploadable data */
	virtual FGPUWindSourceData ToGPUData() const;

	/** Get the wind source type */
	virtual EWindFieldSourceType GetWindType() const PURE_VIRTUAL(UWindFieldSourceComponent::GetWindType, return EWindFieldSourceType::Directional;);

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

private:
	void RegisterWithSubsystem();
	void UnregisterFromSubsystem();
};
