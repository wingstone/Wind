// Copyright TADemo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "LocalWeatherVolume.generated.h"

class UWeatherDataAsset;

/**
 * A volume that overrides the global weather inside a region. Distance-to-boundary
 * falloff is applied so weather blends smoothly at the edge. Multiple overlapping
 * volumes are combined highest-priority-first, each consuming its share of alpha.
 */
UCLASS(BlueprintType, Blueprintable)
class WEATHERSYSTEM_API ALocalWeatherVolume : public AVolume
{
	GENERATED_BODY()

public:
	ALocalWeatherVolume(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	TObjectPtr<UWeatherDataAsset> WeatherAsset;

	/** Falloff distance (world units, cm) — 0 = hard boundary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0.0"))
	float FalloffDistance = 500.0f;

	/** Higher priorities win over lower priorities in overlap regions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	int32 Priority = 0;

	/**
	 * Returns the influence weight at WorldLocation, in [0,1].
	 * 1 = fully inside past the falloff, 0 = fully outside.
	 * Not const because AVolume::EncompassesPoint is non-const in some engine versions.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weather")
	float GetInfluenceWeight(const FVector& WorldLocation);

	virtual void PostRegisterAllComponents() override;
	virtual void PostUnregisterAllComponents() override;
};
