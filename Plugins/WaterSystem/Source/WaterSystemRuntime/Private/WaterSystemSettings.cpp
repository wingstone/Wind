// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WaterSystemSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WaterSystemSettings)

#if WITH_EDITOR
UWaterSystemSettings::FOnUpdateSettings UWaterSystemSettings::OnSettingsChange;

void UWaterSystemSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	OnSettingsChange.Broadcast(this, PropertyChangedEvent.ChangeType);
}
#endif