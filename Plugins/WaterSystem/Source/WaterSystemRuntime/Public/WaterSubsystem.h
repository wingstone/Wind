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

	// --- Water Sampling (CPU) ---

	/** Sample water properties at a world position */
	UFUNCTION(BlueprintCallable, Category = "Water System")
	FWaterSample SampleWaterAtLocation(FVector2D WorldPosition) const;

	/** Check if a position is underwater */
	UFUNCTION(BlueprintPure, Category = "Water System")
	bool IsUnderwater(FVector WorldPosition) const;

	/** Get water surface height at position */
	UFUNCTION(BlueprintPure, Category = "Water System")
	float GetWaterHeight(FVector2D WorldPosition) const;

	// --- Interaction Management ---

	/** Create a water splash/disturbance */
	UFUNCTION(BlueprintCallable, Category = "Water System")
	void CreateSplash(FVector2D Position, float Strength, float Radius, float Duration = 1.0f);

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

	/** Cached water height field (for CPU queries) */
	TArray<float> CachedHeightField;

	/** Cached velocity field (for CPU queries) */
	TArray<FVector2D> CachedVelocityField;

	/** Update scene extension on render thread */
	void UpdateSceneExtension(float DeltaTime);

	/** Send disturbance to render thread */
	void SendDisturbanceToRT(const FVector2D& Position, float Strength, float Radius, float Duration);

	/** Send interaction to render thread */
	void SendInteractionToRT(const FWaterInteractionData& Interaction);

	/** Get the center position for the fluid field */
	FVector2D GetFieldCenterPosition() const;

	/** Convert world position to grid coordinates */
	FIntPoint WorldToGrid(FVector2D WorldPosition) const;

	/** Convert grid coordinates to world position */
	FVector2D GridToWorld(FIntPoint GridPosition) const;

	/** Initialize cached fields */
	void InitializeCachedFields();

	/** Sample from cached field */
	float SampleCachedHeight(FVector2D WorldPosition) const;
	FVector2D SampleCachedVelocity(FVector2D WorldPosition) const;
};
