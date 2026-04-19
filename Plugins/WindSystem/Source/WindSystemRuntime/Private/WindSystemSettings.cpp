// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindSystemSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WindSystemSettings)

#if WITH_EDITOR
UWindSystemSettings::FOnUpdateSettings UWindSystemSettings::OnSettingsChange;

void UWindSystemSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	OnSettingsChange.Broadcast(this, PropertyChangedEvent.ChangeType);
}
#endif
