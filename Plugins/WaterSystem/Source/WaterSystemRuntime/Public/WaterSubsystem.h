// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/UnrealType.h"
#include "WaterFluidTypes.h"
#include "WaterSubsystem.generated.h"

class FWaterFieldSceneExtension;
class UWaterInteractionComponent;
class UWaterFluidConfigComponent;
class UWaterSystemSettings;

/**
 * World subsystem managing 2D water fluid simulation
 * Handles player/object interactions with water using shallow water equations
 */
UCLASS()
class WATERSYSTEMRUNTIME_API UWaterSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	UWaterSubsystem();

	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// UTickableWorldSubsystem interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// --- Fluid Simulation Configuration ---
	FWaterFluidConfig FluidConfig;

	void LoadGlobalFluidConfig(const UWaterSystemSettings* Settings, EPropertyChangeType::Type ChangeType);
	void ResetState();
	
	void RegisterInteractionComponent(UWaterInteractionComponent* Component);
	void UnregisterInteractionComponent(UWaterInteractionComponent* Component);

	void RegisterFluidConfigComponent(UWaterFluidConfigComponent* Component);
	void UnregisterFluidConfigComponent(UWaterFluidConfigComponent* Component);
	
	FWaterFieldSceneExtension* GetSceneExtension() const;
	
private:
	
	UPROPERTY()
	TArray<TObjectPtr<UWaterInteractionComponent>> RegisteredInteractionComponents;
	UPROPERTY()
	TArray<TObjectPtr<UWaterFluidConfigComponent>> RegisteredFluidConfigComponents;

	FIntVector2 LastViewLocationInt = FIntVector2::ZeroValue;
	
	void ScrollWorldGrid();

	/** Update fluid configuration on render thread */
	void UpdateFluidConfig();

	/** Send interaction to render thread */
	void UpdateInteractions();
};
