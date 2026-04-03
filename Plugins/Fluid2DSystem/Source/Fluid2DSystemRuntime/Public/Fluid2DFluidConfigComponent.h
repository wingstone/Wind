// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Fluid2DFluidTypes.h"
#include "Fluid2DFluidConfigComponent.generated.h"

class UFluid2DSubsystem;

/**
 * Component for configuring water fluid settings
 */
UCLASS(ClassGroup = (Fluid2D), meta = (BlueprintSpawnableComponent))
class FLUID2DSYSTEMRUNTIME_API UFluid2DFluidConfigComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UFluid2DFluidConfigComponent();

	/** Fluid simulation configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation")
	FFluid2DFluidConfig FluidConfig;

	FFluid2DFluidConfig GetFluidConfig() const { return FluidConfig; }

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
