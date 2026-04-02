// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "WaterFluidTypes.h"
#include "WaterGlobalFlowComponent.generated.h"

class UWaterSubsystem;
class UTexture2D;

/**
 * Component that drives a global noise-based flow field over the water simulation.
 * Place one in the level to add ambient currents / river-like flow.
 */
UCLASS(ClassGroup = (Water), meta = (BlueprintSpawnableComponent))
class WATERSYSTEMRUNTIME_API UWaterGlobalFlowComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UWaterGlobalFlowComponent();

	/** Enable global flow */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Global Flow")
	bool bEnableGlobalFlow = true;

	/** Noise texture that contain intensity (r) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Global Flow")
	TObjectPtr<UTexture2D> FlowNoiseTexture = nullptr;

    
	/**  flow direction in world space */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Global Flow")
	FVector2D FlowDirection = FVector2D(1.0, 0.0);

	/** Intensity of noise-driven flow */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Global Flow", meta = (ClampMin = "0", UIMin = "0", UIMax = "100"))
	float FlowNoiseIntensity = 1.0f;

	/** Tiling scale of the noise texture over the simulation domain */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Global Flow", meta = (ClampMin = "0.01", UIMin = "0.01", UIMax = "10"))
	float FlowNoiseTiling = 1.0f;

	/** Build a render-thread-safe snapshot of the current flow data */
	FWaterGlobalFlowData GetGlobalFlowData() const;

	bool IsFlowEnabled() const { return bEnableGlobalFlow && FlowNoiseTexture != nullptr; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void RegisterWithSubsystem();
	void UnregisterFromSubsystem();
	UWaterSubsystem* GetWaterSubsystem() const;
};
