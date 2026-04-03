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
class UFluid2DGlobalFlowComponent;
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

	void RegisterGlobalFlowComponent(UFluid2DGlobalFlowComponent* Component);
	void UnregisterGlobalFlowComponent(UFluid2DGlobalFlowComponent* Component);
	
	FFluid2DFieldSceneExtension* GetSceneExtension() const;
	
private:
	
	UPROPERTY()
	TArray<TObjectPtr<UFluid2DInteractionComponent>> RegisteredInteractionComponents;
	UPROPERTY()
	TArray<TObjectPtr<UFluid2DFluidConfigComponent>> RegisteredFluidConfigComponents;
	UPROPERTY()
	TArray<TObjectPtr<UFluid2DGlobalFlowComponent>> RegisteredGlobalFlowComponents;

	FIntVector2 LastScrollTargetLocationInt = FIntVector2::ZeroValue;
	
	void ScrollWorldGrid();

	/** Update fluid configuration on render thread */
	void UpdateFluidConfig();

	/** Send interaction to render thread */
	void UpdateInteractions();

	/** Send global flow data to render thread */
	void UpdateGlobalFlow();
};
