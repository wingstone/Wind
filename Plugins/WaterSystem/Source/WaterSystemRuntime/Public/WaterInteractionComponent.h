// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "WaterFluidTypes.h"
#include "WaterInteractionComponent.generated.h"

class UWaterSubsystem;

/**
 * Component for player/object interaction with water
 * Automatically applies forces, splashes, and queries water state
 */
UCLASS(ClassGroup = (Water), meta = (BlueprintSpawnableComponent))
class WATERSYSTEMRUNTIME_API UWaterInteractionComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UWaterInteractionComponent();

	// --- Interaction Settings ---

	/** Enable automatic water interaction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Interaction")
	bool bEnableInteraction = true;

	/** Interaction radius (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Interaction")
	float InteractionRadius = 100.0f;

	/** Interaction strength multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Interaction")
	float InteractionStrength = 1.0f;

	/** Minimum velocity to trigger splash (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Interaction")
	float SplashVelocityThreshold = 100.0f;

	/** Splash strength multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Interaction")
	float SplashStrength = 50.0f;

	/** Enable continuous wave generation when moving */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Interaction")
	bool bGenerateContinuousWaves = true;

	// --- Manual Control ---

	/** Manually create a splash at current location */
	UFUNCTION(BlueprintCallable, Category = "Water Interaction")
	void CreateSplash(float Strength, float Radius);

	/** Check if currently submerged in water */
	UFUNCTION(BlueprintPure, Category = "Water Interaction")
	bool IsSubmerged() const;

	/** Get submersion ratio (0 = not submerged, 1 = fully submerged) */
	UFUNCTION(BlueprintPure, Category = "Water Interaction")
	float GetSubmersionRatio() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	FVector LastPosition;
	FVector LastVelocity;
	float TimeSinceLastSplash;
	bool bWasSubmerged;

	void RegisterWithSubsystem();
	void UnregisterFromSubsystem();
	void UpdateInteraction(float DeltaTime);
	UWaterSubsystem* GetWaterSubsystem() const;
};
