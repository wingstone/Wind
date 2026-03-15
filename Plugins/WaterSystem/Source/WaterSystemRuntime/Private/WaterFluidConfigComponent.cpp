// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterFluidConfigComponent.h"
#include "WaterSubsystem.h"
#include "Engine/World.h"

UWaterFluidConfigComponent::UWaterFluidConfigComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;
}

void UWaterFluidConfigComponent::BeginPlay()
{
	Super::BeginPlay();
	RegisterWithSubsystem();
}

void UWaterFluidConfigComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromSubsystem();
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void UWaterFluidConfigComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (GetWaterSubsystem())
	{
		GetWaterSubsystem()->ResetState();
	}
}
#endif


void UWaterFluidConfigComponent::RegisterWithSubsystem()
{
	if (UWaterSubsystem* WaterSys = GetWaterSubsystem())
	{
		WaterSys->RegisterFluidConfigComponent(this);
	}

	if (GetWaterSubsystem())
	{
		GetWaterSubsystem()->ResetState();
	}
}

void UWaterFluidConfigComponent::UnregisterFromSubsystem()
{
	if (UWaterSubsystem* WaterSys = GetWaterSubsystem())
	{
		WaterSys->UnregisterFluidConfigComponent(this);
	}
}

UWaterSubsystem* UWaterFluidConfigComponent::GetWaterSubsystem() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetSubsystem<UWaterSubsystem>();
	}
	return nullptr;
}
