// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Fluid2DFluidTypes.h"
#include "Fluid2DFluidConfigComponent.generated.h"

class UFluid2DSubsystem;

/**
 * Component that overrides the configuration of a single solver layer.
 *
 * Solver types coexist, so a level can hold one component per layer: each component picks its
 * SolverType and only the matching config struct below is applied to that layer.
 */
UCLASS(ClassGroup = (Fluid2D), meta = (BlueprintSpawnableComponent))
class FLUID2DSYSTEMRUNTIME_API UFluid2DFluidConfigComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UFluid2DFluidConfigComponent();

	/** Solver layer configured by this component. Only the matching config below is applied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation")
	EFluidSolverType SolverType = EFluidSolverType::ShallowWater;

	/** Shallow water layer config, applied when SolverType is ShallowWater. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (EditCondition = "SolverType == EFluidSolverType::ShallowWater", EditConditionHides))
	FSWFluidConfig ShallowWater;

	/** Navier-Stokes layer config, applied when SolverType is NavierStokes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (EditCondition = "SolverType == EFluidSolverType::NavierStokes", EditConditionHides))
	FNSFluidConfig NavierStokes;

	/** Mask accumulate layer config, applied when SolverType is MaskAccumulate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (EditCondition = "SolverType == EFluidSolverType::MaskAccumulate", EditConditionHides))
	FMAFluidConfig MaskAccumulate;

	/** Configuration of the layer selected by SolverType. */
	const FFluid2DSolverConfigBase& GetSolverConfig() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:

	void RegisterWithSubsystem();
	void UnregisterFromSubsystem();

	UFluid2DSubsystem* GetFluid2DSubsystem() const;
};
