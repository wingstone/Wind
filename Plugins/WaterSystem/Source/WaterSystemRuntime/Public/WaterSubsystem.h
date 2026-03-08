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

	/** Simulation quality (substeps per frame) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Fluid", meta = (ClampMin = "1", ClampMax = "10"))
	int32 SimulationSubsteps = 2;

	// --- Interaction Management ---

	/** Apply continuous interaction (called by WaterInteractionComponent) */
	void ApplyInteraction(const FWaterInteractionData& Interaction);

	/** Register an interaction component */
	void RegisterInteractionComponent(UWaterInteractionComponent* Component);

	/** Unregister an interaction component */
	void UnregisterInteractionComponent(UWaterInteractionComponent* Component);

	/** Get all registered interaction components */
	const TArray<UWaterInteractionComponent*>& GetInteractionComponents() const { return InteractionComponents; }

	/** Get scene extension (for rendering thread) */
	FWaterFieldSceneExtension* GetSceneExtension() const;

private:
	/** All registered interaction components */
	UPROPERTY()
	TArray<TObjectPtr<UWaterInteractionComponent>> InteractionComponents;

	/** Update fluid configuration on render thread */
	void UpdateFluidConfig(float DeltaTime);

	/** Send interaction to render thread */
	void SendInteraction(const FWaterInteractionData& Interaction);

};
