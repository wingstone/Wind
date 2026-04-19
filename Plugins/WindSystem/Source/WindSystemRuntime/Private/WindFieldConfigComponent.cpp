// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFieldConfigComponent.h"
#include "WindSubsystem.h"
#include "Engine/World.h"

UWindFieldConfigComponent::UWindFieldConfigComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;
}

void UWindFieldConfigComponent::BeginPlay()
{
	Super::BeginPlay();
	RegisterWithSubsystem();
}

void UWindFieldConfigComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromSubsystem();
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void UWindFieldConfigComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (UWindSubsystem* Subsystem = GetWindSubsystem())
	{
		Subsystem->ApplyWindFieldConfig();
	}
}
#endif

void UWindFieldConfigComponent::RegisterWithSubsystem()
{
	if (UWindSubsystem* Subsystem = GetWindSubsystem())
	{
		Subsystem->RegisterConfigComponent(this);
		Subsystem->ApplyWindFieldConfig();
	}
}

void UWindFieldConfigComponent::UnregisterFromSubsystem()
{
	if (UWindSubsystem* Subsystem = GetWindSubsystem())
	{
		Subsystem->UnregisterConfigComponent(this);
		Subsystem->ApplyWindFieldConfig();
	}
}

UWindSubsystem* UWindFieldConfigComponent::GetWindSubsystem() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetSubsystem<UWindSubsystem>();
	}
	return nullptr;
}
