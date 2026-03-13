// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WaterFluidTypes.h"
#include "WaterSubsystem.generated.h"

class FWaterFieldSceneExtension;
class UWaterInteractionComponent;

/**
 * World subsystem managing 2D water fluid simulation
 * Handles player/object interactions with water using shallow water equations
 */
UCLASS()
class WATERSYSTEMRUNTIME_API UWaterSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// UTickableWorldSubsystem interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// --- Fluid Simulation Configuration ---

	/** Fluid simulation configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Fluid")
	FWaterFluidConfig FluidConfig;

	/** Enable 2D fluid simulation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Fluid")
	bool bEnableSimulation = true;

	void SetFluidConfig(const FWaterFluidConfig& NewConfig);
	void ApplyInteraction(const FWaterInteractionData& Interaction);
	void ResetState();

	void RegisterInteractionComponent(UWaterInteractionComponent* Component);
	void UnregisterInteractionComponent(UWaterInteractionComponent* Component);

	FWaterFieldSceneExtension* GetSceneExtension() const;

private:

	TArray<FWaterInteractionData> PendingInteractions;
	FIntVector2 LastViewLocationInt = FIntVector2::ZeroValue;

	/** Update fluid configuration on render thread */
	void UpdateFluidConfig();

	/** Send interaction to render thread */
	void UpdateInteractions();
};
