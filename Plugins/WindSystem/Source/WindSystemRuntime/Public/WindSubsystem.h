// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WindFieldTypes.h"
#include "WindSubsystem.generated.h"

class UWindFieldSourceComponent;
class UWindFieldDirectionalComponent;
class FWindFieldSceneExtension;

/**
 * World subsystem managing all wind sources and driving GPU wind field updates.
 *
 * Each frame, collects GPU data from registered wind source components and
 * pushes it to FWindFieldSceneExtension via ENQUEUE_RENDER_COMMAND.
 * The scene extension dispatches compute shaders and exposes the result
 * through the Scene Uniform Buffer for material sampling.
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

	void RegisterWindSource(UWindFieldSourceComponent* Source);
	void UnregisterWindSource(UWindFieldSourceComponent* Source);

	void RegisterDirectionalWind(UWindFieldDirectionalComponent* Component);
	void UnregisterDirectionalWind(UWindFieldDirectionalComponent* Component);

	// --- Configuration ---

	/** Wind field GPU configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Field")
	FWindFieldConfig WindFieldConfig;

	// --- CPU sampling (fallback for AI / character movement) ---

	/** Sample wind at a world position (CPU approximation, not GPU-accurate) */
	UFUNCTION(BlueprintCallable, Category = "Wind System")
	FWindSample SampleWindAtLocation(FVector WorldPosition) const;

	FWindFieldSceneExtension* GetSceneExtension() const;

private:

	UPROPERTY()
	TArray<TObjectPtr<UWindFieldSourceComponent>> RegisteredSources;

	UPROPERTY()
	TObjectPtr<UWindFieldDirectionalComponent> DirectionalWindComponent;

	/** Collect wind source data and push to render thread */
	void UpdateWindField();

	/** Get the camera/player position for field centering */
	FVector GetFieldCenterPosition() const;
};
