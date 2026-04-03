// Copyright Epic Games, Inc. All Rights Reserved.

#include "Fluid2DFluidConfigComponent.h"
#include "Fluid2DSubsystem.h"
#include "Engine/World.h"

UFluid2DFluidConfigComponent::UFluid2DFluidConfigComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;
}

void UFluid2DFluidConfigComponent::BeginPlay()
{
	Super::BeginPlay();
	RegisterWithSubsystem();
}

void UFluid2DFluidConfigComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromSubsystem();
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void UFluid2DFluidConfigComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (GetFluid2DSubsystem())
	{
		GetFluid2DSubsystem()->ResetState();
	}
}
#endif

void UFluid2DFluidConfigComponent::RegisterWithSubsystem()
{
	if (UFluid2DSubsystem* Fluid2DSys = GetFluid2DSubsystem())
	{
		Fluid2DSys->RegisterFluidConfigComponent(this);
		 Fluid2DSys->ResetState();
	}
}

void UFluid2DFluidConfigComponent::UnregisterFromSubsystem()
{
	if (UFluid2DSubsystem* Fluid2DSys = GetFluid2DSubsystem())
	{
		Fluid2DSys->UnregisterFluidConfigComponent(this);
	}
}

UFluid2DSubsystem* UFluid2DFluidConfigComponent::GetFluid2DSubsystem() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetSubsystem<UFluid2DSubsystem>();
	}
	return nullptr;
}
