// Copyright Epic Games, Inc. All Rights Reserved.

#include "Fluid2DGlobalFlowComponent.h"
#include "Fluid2DSubsystem.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

UFluid2DGlobalFlowComponent::UFluid2DGlobalFlowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;
}

void UFluid2DGlobalFlowComponent::BeginPlay()
{
	Super::BeginPlay();
	RegisterWithSubsystem();
}

void UFluid2DGlobalFlowComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromSubsystem();
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void UFluid2DGlobalFlowComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (GetFluid2DSubsystem())
	{
		GetFluid2DSubsystem()->ResetState();
	}
}
#endif

FFluid2DGlobalFlowData UFluid2DGlobalFlowComponent::GetGlobalFlowData() const
{
	FFluid2DGlobalFlowData Data;
    Data.FlowDirection = FlowDirection.GetSafeNormal();
	Data.FlowNoiseIntensityMin = FlowNoiseIntensityMin;
	Data.FlowNoiseIntensityMax = FlowNoiseIntensityMax;
	Data.FlowNoiseTiling = FlowNoiseTiling;

	if (FlowNoiseTexture && FlowNoiseTexture->GetResource())
	{
		Data.FlowNoiseTextureRHI = FlowNoiseTexture->GetResource()->TextureRHI;
	}

	return Data;
}

void UFluid2DGlobalFlowComponent::RegisterWithSubsystem()
{
	if (UFluid2DSubsystem* Fluid2DSys = GetFluid2DSubsystem())
	{
		Fluid2DSys->RegisterGlobalFlowComponent(this);
        Fluid2DSys->ResetState();
	}
}

void UFluid2DGlobalFlowComponent::UnregisterFromSubsystem()
{
	if (UFluid2DSubsystem* Fluid2DSys = GetFluid2DSubsystem())
	{
		Fluid2DSys->UnregisterGlobalFlowComponent(this);
	}
}

UFluid2DSubsystem* UFluid2DGlobalFlowComponent::GetFluid2DSubsystem() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetSubsystem<UFluid2DSubsystem>();
	}
	return nullptr;
}
