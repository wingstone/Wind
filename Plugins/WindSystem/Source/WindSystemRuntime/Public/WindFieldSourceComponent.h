// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "WindFieldTypes.h"
#include "WindFieldSourceComponent.generated.h"

class UWindSubsystem;

/**
 * Base class for all wind field source components.
 * Provides the interface for GPU data upload and auto-registers with UWindSubsystem.
 * Subclasses define their own parameters.
 */
UCLASS(Abstract, ClassGroup = (Wind), meta = (BlueprintSpawnableComponent))
class WINDSYSTEMRUNTIME_API UWindFieldSourceComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UWindFieldSourceComponent();

	// --- Interface ---

	/** Convert component parameters to GPU-uploadable data */
	virtual FGPUWindSourceData ToGPUData() const PURE_VIRTUAL(UWindFieldSourceComponent::ToGPUData, return {};);

	/** Get the wind source type */
	virtual EWindFieldSourceType GetWindType() const PURE_VIRTUAL(UWindFieldSourceComponent::GetWindType, return EWindFieldSourceType::Directional;);

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

private:
	void RegisterWithSubsystem();
	void UnregisterFromSubsystem();
};
