// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFieldDirectionalComponent.h"
#include "WindSubsystem.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

UWindFieldDirectionalComponent::UWindFieldDirectionalComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;
	SetIsReplicatedByDefault(false);

#if WITH_EDITORONLY_DATA
	bVisualizeComponent = true;
#endif
}

void UWindFieldDirectionalComponent::OnRegister()
{
	Super::OnRegister();
	RegisterWithSubsystem();
}

void UWindFieldDirectionalComponent::OnUnregister()
{
	UnregisterFromSubsystem();
	Super::OnUnregister();
}

#if WITH_EDITOR
void UWindFieldDirectionalComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

FWindDirectionalData UWindFieldDirectionalComponent::GetDirectionalData() const
{
	FWindDirectionalData Data;
	Data.bEnabled = bEnableDirectionalWind;
	Data.WindDirection = FVector3f(GetForwardVector());
	Data.Strength = Strength;
	Data.NoiseStrength = NoiseStrength;

	if (HasNoiseTexture())
	{
		Data.NoiseTextureRHI = WindNoiseTexture->GetResource()->TextureRHI;
	}
	Data.Tiling = WindNoiseTiling;
	Data.IntensityMin = WindNoiseIntensityMin;
	Data.IntensityMax = WindNoiseIntensityMax;
	Data.ScrollSpeed = WindNoiseScrollSpeed;

	return Data;
}

bool UWindFieldDirectionalComponent::IsWindEnabled() const
{
	return bEnableDirectionalWind && IsActive();
}

bool UWindFieldDirectionalComponent::HasNoiseTexture() const
{
	return WindNoiseTexture != nullptr
		&& WindNoiseTexture->GetResource() != nullptr
		&& WindNoiseTexture->GetResource()->TextureRHI != nullptr;
}

void UWindFieldDirectionalComponent::RegisterWithSubsystem()
{
	if (UWindSubsystem* Sub = GetWindSubsystem())
	{
		Sub->RegisterDirectionalWind(this);
	}
}

void UWindFieldDirectionalComponent::UnregisterFromSubsystem()
{
	if (UWindSubsystem* Sub = GetWindSubsystem())
	{
		Sub->UnregisterDirectionalWind(this);
	}
}

UWindSubsystem* UWindFieldDirectionalComponent::GetWindSubsystem() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetSubsystem<UWindSubsystem>();
	}
	return nullptr;
}
