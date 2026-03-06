// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterInteractionComponent.h"
#include "WaterSubsystem.h"
#include "Engine/World.h"

UWaterInteractionComponent::UWaterInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;
	TimeSinceLastSplash = 0.0f;
	bWasSubmerged = false;
}

void UWaterInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	LastPosition = GetComponentLocation();
	RegisterWithSubsystem();
}

void UWaterInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromSubsystem();
	Super::EndPlay(EndPlayReason);
}

void UWaterInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnableInteraction)
		return;

	UpdateInteraction(DeltaTime);

	LastPosition = GetComponentLocation();
	TimeSinceLastSplash += DeltaTime;
}

void UWaterInteractionComponent::RegisterWithSubsystem()
{
	if (UWaterSubsystem* WaterSys = GetWaterSubsystem())
	{
		WaterSys->RegisterInteractionComponent(this);
	}
}

void UWaterInteractionComponent::UnregisterFromSubsystem()
{
	if (UWaterSubsystem* WaterSys = GetWaterSubsystem())
	{
		WaterSys->UnregisterInteractionComponent(this);
	}
}

void UWaterInteractionComponent::UpdateInteraction(float DeltaTime)
{
	UWaterSubsystem* WaterSys = GetWaterSubsystem();
	if (!WaterSys)
		return;

	FVector CurrentPosition = GetComponentLocation();
	FVector CurrentVelocity = (CurrentPosition - LastPosition) / DeltaTime;
	bool bIsSubmerged = IsSubmerged();

	// Check for splash (entering water)
	if (bIsSubmerged && !bWasSubmerged)
	{
		float Speed = CurrentVelocity.Size();
		if (Speed > SplashVelocityThreshold)
		{
			CreateSplash(SplashStrength * (Speed / SplashVelocityThreshold), InteractionRadius);
		}
	}

	// Continuous wave generation
	if (bIsSubmerged && bGenerateContinuousWaves)
	{
		FVector2D Velocity2D(CurrentVelocity.X, CurrentVelocity.Y);
		float Speed2D = Velocity2D.Size();

		if (Speed2D > 10.0f) // Minimum speed to generate waves
		{
			FWaterInteractionData Interaction;
			Interaction.Position = FVector2D(CurrentPosition.X, CurrentPosition.Y);
			Interaction.Force = Velocity2D.GetSafeNormal() * InteractionStrength;
			Interaction.Radius = InteractionRadius;
			Interaction.Strength = InteractionStrength * (Speed2D / 100.0f);

			WaterSys->ApplyInteraction(Interaction);
		}
	}

	bWasSubmerged = bIsSubmerged;
	LastVelocity = CurrentVelocity;
}

void UWaterInteractionComponent::CreateSplash(float Strength, float Radius)
{
	UWaterSubsystem* WaterSys = GetWaterSubsystem();
	if (!WaterSys)
		return;

	FVector Position = GetComponentLocation();
	FVector2D Position2D(Position.X, Position.Y);

	WaterSys->CreateSplash(Position2D, Strength, Radius, 1.0f);
	TimeSinceLastSplash = 0.0f;
}

bool UWaterInteractionComponent::IsSubmerged() const
{
	return false;
}

float UWaterInteractionComponent::GetSubmersionRatio() const
{
	return 0.0f;
}

UWaterSubsystem* UWaterInteractionComponent::GetWaterSubsystem() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetSubsystem<UWaterSubsystem>();
	}
	return nullptr;
}
