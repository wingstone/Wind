// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterGlobalFlowComponent.h"
#include "WaterSubsystem.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

UWaterGlobalFlowComponent::UWaterGlobalFlowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;
}

void UWaterGlobalFlowComponent::BeginPlay()
{
	Super::BeginPlay();
	RegisterWithSubsystem();
}

void UWaterGlobalFlowComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromSubsystem();
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void UWaterGlobalFlowComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (GetWaterSubsystem())
	{
		GetWaterSubsystem()->ResetState();
	}
}
#endif

FWaterGlobalFlowData UWaterGlobalFlowComponent::GetGlobalFlowData() const
{
	FWaterGlobalFlowData Data;
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

void UWaterGlobalFlowComponent::RegisterWithSubsystem()
{
	if (UWaterSubsystem* WaterSys = GetWaterSubsystem())
	{
		WaterSys->RegisterGlobalFlowComponent(this);
        WaterSys->ResetState();
	}
}

void UWaterGlobalFlowComponent::UnregisterFromSubsystem()
{
	if (UWaterSubsystem* WaterSys = GetWaterSubsystem())
	{
		WaterSys->UnregisterGlobalFlowComponent(this);
	}
}

UWaterSubsystem* UWaterGlobalFlowComponent::GetWaterSubsystem() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetSubsystem<UWaterSubsystem>();
	}
	return nullptr;
}
