// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WindFieldTypes.h"
#include "WindSubsystem.generated.h"

class UWindFieldSourceComponent;
class FWindFieldProxy;

/**
 * World subsystem managing all wind sources and driving GPU wind field updates.
 * Collects wind source data each frame and pushes it to the render thread
 * via FWindFieldProxy for GPU compute shader processing.
 */
UCLASS()
class WINDSYSTEMRUNTIME_API UWindSubsystem : public UTickableWorldSubsystem
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

	// --- Wind source management ---

	/** Register a wind source component with the subsystem */
	void RegisterWindSource(UWindFieldSourceComponent* Source);

	/** Unregister a wind source component */
	void UnregisterWindSource(UWindFieldSourceComponent* Source);

	/** Get all registered wind sources */
	const TArray<UWindFieldSourceComponent*>& GetRegisteredSources() const { return RegisteredSources; }

	// --- Configuration ---

	/** Wind field GPU configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Field")
	FWindFieldConfig WindFieldConfig;

	// --- CPU sampling (fallback / AI / movement) ---

	/** Sample wind at a world position (CPU approximation, not GPU-accurate) */
	UFUNCTION(BlueprintCallable, Category = "Wind System")
	FWindSample SampleWindAtLocation(FVector WorldPosition) const;

	/** Get the proxy for render thread communication */
	TSharedPtr<FWindFieldProxy> GetFieldProxy() const { return FieldProxy; }

private:
	/** All currently registered wind source components */
	UPROPERTY()
	TArray<TObjectPtr<UWindFieldSourceComponent>> RegisteredSources;

	/** Thread-safe bridge to render thread */
	TSharedPtr<FWindFieldProxy> FieldProxy;

	/** Collect wind source data and push to render thread */
	void UpdateGPUData();

	/** Get the camera/player position for field centering */
	FVector GetFieldCenterPosition() const;
};
