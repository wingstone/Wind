// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/UnrealType.h"
#include "Fluid2DFluidTypes.h"
#include "Fluid2DSubsystem.generated.h"

class FFluid2DFieldSceneExtension;
class UFluid2DInteractionComponent;
class UFluid2DFluidConfigComponent;
class AFluid2DFlowVolume;
class UFluid2DSystemSettings;

/**
 * World subsystem managing 2D water fluid simulation
 * Handles player/object interactions with water using shallow water equations
 */
UCLASS()
class FLUID2DSYSTEMRUNTIME_API UFluid2DSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	UFluid2DSubsystem();

	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// UTickableWorldSubsystem interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// --- Fluid Simulation Configuration ---
	FFluid2DFluidConfig FluidConfig;

	void LoadGlobalFluidConfig(const UFluid2DSystemSettings* Settings, EPropertyChangeType::Type ChangeType);
	void ResetState();
	
	void RegisterInteractionComponent(UFluid2DInteractionComponent* Component);
	void UnregisterInteractionComponent(UFluid2DInteractionComponent* Component);

	void RegisterFluidConfigComponent(UFluid2DFluidConfigComponent* Component);
	void UnregisterFluidConfigComponent(UFluid2DFluidConfigComponent* Component);

	void InsertFlowVolume(AFluid2DFlowVolume* Volume);
	void RemoveFlowVolume(AFluid2DFlowVolume* Volume);

	/**
	 * Sample the flow velocity at a world-space location (CPU-side approximation).
	 * Uses the same priority-first + BlendRadius blend as the render-thread flow, but
	 * substitutes the mean of FlowNoiseIntensityMin/Max for the noise texture value
	 * since we do not sample the noise texture on the CPU.
	 * Returns FVector2D::ZeroVector when no volume affects the point.
	 */
	FVector2D GetFlowVelocityAt(const FVector& WorldPos) const;

	FFluid2DFieldSceneExtension* GetSceneExtension() const;

private:

	UPROPERTY()
	TArray<TObjectPtr<UFluid2DInteractionComponent>> RegisteredInteractionComponents;
	UPROPERTY()
	TArray<TObjectPtr<UFluid2DFluidConfigComponent>> RegisteredFluidConfigComponents;
	UPROPERTY()
	TArray<TObjectPtr<AFluid2DFlowVolume>> FlowVolumes;

	FIntVector2 LastScrollTargetLocationInt = FIntVector2::ZeroValue;

	/** Compute the world-space location the simulation is sampling around this frame (player pawn / camera / last rendered view). */
	FVector GetSampleLocation() const;

	/**
	 * Resolve the flow volume that should drive the flow at WorldPos.
	 * Returns the winning volume (or nullptr) and the blend weight in [0, 1].
	 */
	AFluid2DFlowVolume* ResolveFlowVolumeAt(const FVector& WorldPos, float& OutBlendWeight) const;

	void ScrollWorldGrid();

	/** Update fluid configuration on render thread */
	void UpdateFluidConfig();

	/** Send interaction to render thread */
	void UpdateInteractions();

	/** Send global flow data to render thread */
	void UpdateGlobalFlow();
};
