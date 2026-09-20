// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFieldConfigComponent.h"
#include "WindSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogWindConfig, Log, All);

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
	UWorld* World = GetWorld();
	UE_LOG(LogWindConfig, Warning, TEXT("[WindConfig] Register attempt: Owner=%s World=%s WorldType=%d"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(World),
		World ? (int32)World->WorldType : -1);

	if (UWindSubsystem* Subsystem = GetWindSubsystem())
	{
		Subsystem->RegisterConfigComponent(this);
		Subsystem->ApplyWindFieldConfig();
		UE_LOG(LogWindConfig, Warning, TEXT("[WindConfig]   -> registered OK"));
	}
	else
	{
		UE_LOG(LogWindConfig, Warning, TEXT("[WindConfig]   -> NO SUBSYSTEM"));
	}
}

void UWindFieldConfigComponent::UnregisterFromSubsystem()
{
	if (UWindSubsystem* Subsystem = GetWindSubsystem())
	{
		Subsystem->UnregisterConfigComponent(this);
		Subsystem->ApplyWindFieldConfig();
		UE_LOG(LogWindConfig, Warning, TEXT("[WindConfig] Unregistered: Owner=%s"), *GetNameSafe(GetOwner()));
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
