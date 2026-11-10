// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Fluid2DFluidTypes.h"
#include "Fluid2DInteractionComponent.generated.h"

class UFluid2DSubsystem;

/**
 * Component for player/object interaction with water
 * Automatically applies forces, splashes, and queries water state
 */
UCLASS(ClassGroup = (Fluid2D), meta = (BlueprintSpawnableComponent))
class FLUID2DSYSTEMRUNTIME_API UFluid2DInteractionComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UFluid2DInteractionComponent();

	// --- Interaction Settings ---

	/** Enable automatic water interaction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid2D Interaction Control")
	bool bEnableInteraction = true;

	/**
	 * Solver layers this component interacts with.
	 * Empty = every enabled layer, so an object disturbs all coexisting fluids by default.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid2D Interaction Control")
	TSet<EFluidSolverType> TargetSolverTypes;

	/** Minimum speed required to generate interaction effects */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid2D Interaction Control")
	float MinSpeedForInteraction = 10.0f;

	/** Interaction shape type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	ESourceShapeType ShapeType = ESourceShapeType::Disk;

	/** Interaction emission type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	ESourceEmissionType EmissionType = ESourceEmissionType::Directional;

	/** Interaction radius */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float Radius = 100.0f;

	/** Interaction width (only used for ring shape) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float Width = 0.0f; // Only used for ring shape

	/** Whether to use a custom interaction direction */
	// ScriptName keeps this out of the way of CustomDirection below; in Python both names
	// sanitize to custom_direction, and Python refuses to expose the duplicate.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ScriptName = "use_custom_direction"))
	bool bCustomDirection = false;

	/** Custom interaction direction in world space, used when bCustomDirection is true */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (EditCondition = "bCustomDirection"))
	FVector2D CustomDirection = FVector2D(1.0, 0.0);

	/** Whether to use component velocity for interaction force calculation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bUseComponentVelocity = true;

	/** Interaction direction strength paramter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float DirectionalStrength = 1.0f; // Only used for directional emission

	/** Interaction omni strength paramter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float OmniStrength = 0.0f;	// Only used for omni emission

	/** Interaction vortex strength paramter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float VortexStrength = 0.0f;	// Only used for vortex emission
	
	/** Interaction Power falloff */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (UIMin = "0.001", UIMax = "10", ClampMax = "10", ClampMin = "0.001"))
	float PowerFalloff = 1.0f;

	/** Interaction height intensity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float HeightIntensity = 1.0f;

public:
	FFluid2DInteractionData GetCurrentInteractionData() const { return CurrentInteractionData; }
	bool IsInteractionUseful() const { return bEnableInteraction && bIsInteractionUseful; }

	/** True when the component is below the rest level of the given solver layer. */
	UFUNCTION(BlueprintPure, Category = "Fluid2D Interaction Control")
	bool IsSubmergedIn(EFluidSolverType SolverType) const;

	/** True when the component is below the rest level of at least one targeted solver layer. */
	UFUNCTION(BlueprintPure, Category = "Fluid2D Interaction Control")
	bool IsSubmerged() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Cached previous world position for velocity estimation */
	FVector LastPosition = FVector::ZeroVector;
	FFluid2DInteractionData CurrentInteractionData;
	bool bIsInteractionUseful;

	void RegisterWithSubsystem();
	void UnregisterFromSubsystem();
	
	void UpdateInteraction(float DeltaTime);
	bool IsSubmergedAt(const FVector& Position) const;

	/** Solver layer whose flow drives the relative-velocity calculation: lowest enabled target layer, else the primary one. */
	EFluidSolverType GetFlowSolverType(const UFluid2DSubsystem* Fluid2DSys) const;
	UFluid2DSubsystem* GetFluid2DSubsystem() const;
};
