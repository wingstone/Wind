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

	/** Interaction shape type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	ESourceShapeType ShapeType = ESourceShapeType::Disk;

	/** Interaction emission type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	ESourceEmissionType EmissionType = ESourceEmissionType::Directional;

	/** Interaction force/velocity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FVector2D ForceDirection = FVector2D::ZeroVector;

	/** Interaction radius */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float Radius = 100.0f;

	/** Interaction width (only used for ring shape) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float Width = 0.0f; // Only used for ring shape

	/** Interaction direction strength paramter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float DisectionalStrength = 1.0f; // Only used for directional emission

	/** Interaction omni strength paramter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float OmniStrength = 0.0f;	// Only used for omni emission

	/** Interaction vortex strength paramter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float VortexStrength = 0.0f;	// Only used for vortex emission
	
	/** Interaction Gaussian falloff */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float GaussianFalloff = 0.0f; // Only used for disk shape


protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Cached previous world position for velocity estimation */
	FVector LastPosition = FVector::ZeroVector;

	void RegisterWithSubsystem();
	void UnregisterFromSubsystem();
	void UpdateInteraction(float DeltaTime);
	bool IsSubmerged(FVector CurrentPosition) const;
	UWaterSubsystem* GetWaterSubsystem() const;
};
