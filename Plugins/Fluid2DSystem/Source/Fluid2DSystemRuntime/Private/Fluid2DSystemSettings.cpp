// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Fluid2DSystemSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(Fluid2DSystemSettings)

#if WITH_EDITOR
UFluid2DSystemSettings::FOnUpdateSettings UFluid2DSystemSettings::OnSettingsChange;

void UFluid2DSystemSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	OnSettingsChange.Broadcast(this, PropertyChangedEvent.ChangeType);
}
#endif