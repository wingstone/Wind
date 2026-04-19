// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "WindFieldTypes.h"
#include "WindFieldConfigComponent.generated.h"

class UWindSubsystem;

/**
 * Component for overriding the wind field configuration per-level.
 * Registers with UWindSubsystem on BeginPlay; the subsystem uses this
 * config instead of the Project Settings default when present.
 */
UCLASS(ClassGroup = (Wind), meta = (BlueprintSpawnableComponent))
class WINDSYSTEMRUNTIME_API UWindFieldConfigComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UWindFieldConfigComponent();

	/** Wind field configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Field")
	FWindFieldConfig WindFieldConfig;

	FWindFieldConfig GetWindFieldConfig() const { return WindFieldConfig; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void RegisterWithSubsystem();
	void UnregisterFromSubsystem();

	UWindSubsystem* GetWindSubsystem() const;
};
