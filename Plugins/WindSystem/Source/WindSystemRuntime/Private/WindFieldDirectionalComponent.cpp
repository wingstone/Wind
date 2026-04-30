// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFieldDirectionalComponent.h"
#include "WindSubsystem.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

UWindFieldDirectionalComponent::UWindFieldDirectionalComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bAutoActivate = true;
	bTickInEditor = true;
	SetIsReplicatedByDefault(false);

#if WITH_EDITORONLY_DATA
	bVisualizeComponent = true;
#endif
}

void UWindFieldDirectionalComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if WITH_EDITOR
	if (GetWorld())
	{
		DrawDebug();
	}
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

void UWindFieldDirectionalComponent::DrawDebug(float Lifetime) const
{
	const UWorld* World = GetWorld();
	if (!World || !bEnableDirectionalWind)
	{
		return;
	}

	const FVector Center = GetComponentLocation();
	const FVector WindDir = GetForwardVector();
	const FColor ArrowColor = FColor::Magenta;
	constexpr float Thickness = 2.0f;

	// Main direction arrow
	const float ArrowLength = 200.0f;
	DrawDebugDirectionalArrow(World, Center, Center + WindDir * ArrowLength, 25.0f, ArrowColor, false, Lifetime, 0, Thickness);

	// Grid of smaller arrows showing the directional field
	const FVector Right = FVector::CrossProduct(WindDir, FMath::Abs(WindDir.Z) < 0.99f ? FVector::UpVector : FVector::RightVector).GetSafeNormal();
	const FVector Up = FVector::CrossProduct(Right, WindDir).GetSafeNormal();

	constexpr float Spacing = 150.0f;
	constexpr int32 GridHalf = 2;
	const float SmallArrowLen = 100.0f;

	for (int32 r = -GridHalf; r <= GridHalf; ++r)
	{
		for (int32 u = -GridHalf; u <= GridHalf; ++u)
		{
			if (r == 0 && u == 0)
			{
				continue; // skip center, already drawn
			}
			FVector Offset = Right * (r * Spacing) + Up * (u * Spacing);
			FVector Start = Center + Offset;
			DrawDebugDirectionalArrow(World, Start, Start + WindDir * SmallArrowLen, 10.0f, FColor(180, 80, 180), false, Lifetime, 0, Thickness * 0.5f);
		}
	}
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
