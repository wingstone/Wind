// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/UnrealType.h"
#include "WindFieldTypes.h"
#include "WindSubsystem.generated.h"

class UWindFieldSourceComponent;
class UWindFieldDirectionalComponent;
class UWindFieldConfigComponent;
class UWindSystemSettings;
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

	UWindSubsystem();

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

	void RegisterConfigComponent(UWindFieldConfigComponent* Component);
	void UnregisterConfigComponent(UWindFieldConfigComponent* Component);

	// --- Configuration ---

	/** Active wind field configuration (loaded from Project Settings, overridable per-level) */
	FWindFieldConfig WindFieldConfig;

	/** Re-evaluate the active config: use level-placed config component if present, else Project Settings default. */
	void ApplyWindFieldConfig();

	/** Called when Project Settings change in the editor */
	void LoadGlobalWindFieldConfig(const UWindSystemSettings* Settings, EPropertyChangeType::Type ChangeType);

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

	UPROPERTY()
	TArray<TObjectPtr<UWindFieldConfigComponent>> RegisteredConfigComponents;

	/** Collect wind source data and push to render thread */
	void UpdateWindField();

	/** Get the camera/player position for field centering */
	FVector GetFieldCenterPosition() const;
};
