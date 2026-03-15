// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "WaterFluidTypes.h"
#include "WaterFluidConfigComponent.generated.h"

class UWaterSubsystem;

/**
 * Component for configuring water fluid settings
 */
UCLASS(ClassGroup = (Water), meta = (BlueprintSpawnableComponent))
class WATERSYSTEMRUNTIME_API UWaterFluidConfigComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UWaterFluidConfigComponent();

	/** Fluid simulation configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation")
	FWaterFluidConfig FluidConfig;

	FWaterFluidConfig GetFluidConfig() const { return FluidConfig; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:

	void RegisterWithSubsystem();
	void UnregisterFromSubsystem();

	UWaterSubsystem* GetWaterSubsystem() const;
};
