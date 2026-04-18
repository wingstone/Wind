// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFieldSourceComponent.h"
#include "WindSubsystem.h"
#include "Engine/World.h"

UWindFieldSourceComponent::UWindFieldSourceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;
	SetIsReplicatedByDefault(false);

#if WITH_EDITORONLY_DATA
	bVisualizeComponent = true;
#endif
}

void UWindFieldSourceComponent::OnRegister()
{
	Super::OnRegister();
	RegisterWithSubsystem();
}

void UWindFieldSourceComponent::OnUnregister()
{
	UnregisterFromSubsystem();
	Super::OnUnregister();
}

void UWindFieldSourceComponent::RegisterWithSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UWindSubsystem* Subsystem = World->GetSubsystem<UWindSubsystem>())
		{
			Subsystem->RegisterWindSource(this);
		}
	}
}

void UWindFieldSourceComponent::UnregisterFromSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UWindSubsystem* Subsystem = World->GetSubsystem<UWindSubsystem>())
		{
			Subsystem->UnregisterWindSource(this);
		}
	}
}
