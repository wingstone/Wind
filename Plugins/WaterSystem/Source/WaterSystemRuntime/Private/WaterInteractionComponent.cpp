// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterInteractionComponent.h"
#include "WaterSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"

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
	ApplyPhysicsForces(DeltaTime);

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

void UWaterInteractionComponent::ApplyPhysicsForces(float DeltaTime)
{
	if (!bApplyBuoyancy && !bApplyWaterDrag)
		return;

	AActor* Owner = GetOwner();
	if (!Owner)
		return;

	// Find physics component
	UPrimitiveComponent* PhysicsComp = Cast<UPrimitiveComponent>(Owner->GetRootComponent());
	if (!PhysicsComp || !PhysicsComp->IsSimulatingPhysics())
		return;

	if (bApplyBuoyancy)
	{
		ApplyBuoyancyForce(PhysicsComp);
	}

	if (bApplyWaterDrag)
	{
		ApplyDragForce(PhysicsComp);
	}
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

FWaterSample UWaterInteractionComponent::GetWaterSample() const
{
	UWaterSubsystem* WaterSys = GetWaterSubsystem();
	if (WaterSys)
	{
		FVector Position = GetComponentLocation();
		return WaterSys->SampleWaterAtLocation(FVector2D(Position.X, Position.Y));
	}
	return FWaterSample();
}

bool UWaterInteractionComponent::IsSubmerged() const
{
	UWaterSubsystem* WaterSys = GetWaterSubsystem();
	if (WaterSys)
	{
		return WaterSys->IsUnderwater(GetComponentLocation());
	}
	return false;
}

float UWaterInteractionComponent::GetSubmersionRatio() const
{
	UWaterSubsystem* WaterSys = GetWaterSubsystem();
	if (!WaterSys)
		return 0.0f;

	FVector Position = GetComponentLocation();
	FVector2D Position2D(Position.X, Position.Y);
	float WaterHeight = WaterSys->GetWaterHeight(Position2D);

	// Simple ratio based on position relative to water surface
	AActor* Owner = GetOwner();
	if (!Owner)
		return 0.0f;

	FVector Origin, Extent;
	Owner->GetActorBounds(false, Origin, Extent);

	float ObjectBottom = Origin.Z - Extent.Z;
	float ObjectTop = Origin.Z + Extent.Z;

	if (WaterHeight < ObjectBottom)
		return 0.0f; // Above water

	if (WaterHeight > ObjectTop)
		return 1.0f; // Fully submerged

	return (WaterHeight - ObjectBottom) / (2.0f * Extent.Z);
}

void UWaterInteractionComponent::ApplyBuoyancyForce(UPrimitiveComponent* Component)
{
	if (!Component || !Component->IsSimulatingPhysics())
		return;

	float SubmersionRatio = GetSubmersionRatio();
	if (SubmersionRatio <= 0.0f)
		return;

	UWaterSubsystem* WaterSys = GetWaterSubsystem();
	if (!WaterSys)
		return;

	// Archimedes' principle: F = ρ * V * g
	float WaterDensity = WaterSys->FluidConfig.Density;
	float Gravity = WaterSys->FluidConfig.Gravity;
	float Volume = Component->GetMass(); // Approximate volume from mass

	float BuoyancyForce = WaterDensity * Volume * Gravity * SubmersionRatio * BuoyancyStrength;
	FVector UpwardForce = FVector(0, 0, BuoyancyForce);

	Component->AddForce(UpwardForce);
}

void UWaterInteractionComponent::ApplyDragForce(UPrimitiveComponent* Component)
{
	if (!Component || !Component->IsSimulatingPhysics())
		return;

	float SubmersionRatio = GetSubmersionRatio();
	if (SubmersionRatio <= 0.0f)
		return;

	FVector Velocity = Component->GetPhysicsLinearVelocity();
	if (Velocity.SizeSquared() < 1.0f)
		return;

	// Drag force: F = -0.5 * ρ * Cd * A * v² * v_normalized
	float WaterDensity = 1.0f; // Normalize
	float Speed = Velocity.Size();
	FVector DragForce = -0.5f * WaterDensity * DragCoefficient * Speed * Speed * Velocity.GetSafeNormal() * SubmersionRatio;

	Component->AddForce(DragForce);
}

UWaterSubsystem* UWaterInteractionComponent::GetWaterSubsystem() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetSubsystem<UWaterSubsystem>();
	}
	return nullptr;
}
