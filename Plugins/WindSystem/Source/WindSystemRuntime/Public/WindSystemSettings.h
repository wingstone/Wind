// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/UnrealType.h"
#include "WindFieldTypes.h"
#include "WindSystemSettings.generated.h"

/**
 * Global wind system settings accessible via Project Settings → Game → Wind System.
 * Provides default WindFieldConfig values that can be overridden per-level
 * by placing an AWindFieldConfigActor in the level.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Wind System"))
class WINDSYSTEMRUNTIME_API UWindSystemSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Default wind field configuration */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Wind Field")
	FWindFieldConfig DefaultWindFieldConfig;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	static const UWindSystemSettings* Get() { return GetDefault<UWindSystemSettings>(); }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnUpdateSettings, const UWindSystemSettings* /*Settings*/, EPropertyChangeType::Type /*ChangeType*/);
	static FOnUpdateSettings OnSettingsChange;
#endif
};
