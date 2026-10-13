// Copyright Epic Games, Inc. All Rights Reserved.

#include "PetalPickup.h"
#include "Components/BillboardComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PetalPickupCollectableComponent.h"
#include "PetalPickupFieldComponent.h"
#include "PetalTypes.h"
#include "Wind.h"

APetalPickup::APetalPickup()
{
	// The actor is a marker: it never ticks on its own, never blocks anything, and never replicates.
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Handle = CreateDefaultSubobject<UBillboardComponent>(TEXT("Handle"));
	Handle->SetupAttachment(SceneRoot);
	Handle->SetHiddenInGame(true);
	Handle->bIsScreenSizeScaled = true;
	Handle->ScreenSize = 0.0025f;

	HeroMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeroMesh"));
	HeroMesh->SetupAttachment(SceneRoot);
	HeroMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeroMesh->SetGenerateOverlapEvents(false);
	HeroMesh->SetCastShadow(false);
	HeroMesh->SetVisibility(false);

	Collectable = CreateDefaultSubobject<UPetalPickupCollectableComponent>(TEXT("Collectable"));
	Collectable->SetupAttachment(SceneRoot);
	Collectable->SetComponentTickEnabled(false);
}

void APetalPickup::BeginPlay()
{
	Super::BeginPlay();

	if (bAuthoringOnly)
	{
		// Hero petals stay actors: they show their own mesh and collect themselves.
		HeroMesh->SetVisibility(true);
		Collectable->SetComponentTickEnabled(Collectable->bEnable);
		return;
	}

	if (bCaptureOnBeginPlay)
	{
		RegisterToNearestField();
	}
}

void APetalPickup::RegisterToNearestField()
{
	if (bCaptured)
	{
		return;
	}

	UPetalPickupFieldComponent* Field = UPetalPickupFieldComponent::FindNearestField(GetWorld(), GetActorLocation());
	if (!Field)
	{
		UE_LOG(LogWind, Warning, TEXT("Petal '%s' has no petal field to join; it will only be collectable once one exists."), *GetName());
		return;
	}

	Field->CapturePickup(this);
}

void APetalPickup::OnCaptured()
{
	bCaptured = true;

	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
	SetActorEnableCollision(false);

	if (Handle)
	{
		Handle->SetVisibility(false);
	}

	if (HeroMesh)
	{
		HeroMesh->SetVisibility(false);
	}

	if (Collectable)
	{
		Collectable->SetComponentTickEnabled(false);
	}
}

float APetalPickup::GetPickupPhase() const
{
	// Derived from the placement so the same level always looks the same, and so neighbouring
	// petals are out of step without the designer having to set anything.
	const FVector Location = GetActorLocation();
	const int32 LocationHash = static_cast<int32>(FMath::FloorToInt32(Location.X))
		^ (static_cast<int32>(FMath::FloorToInt32(Location.Y)) * 3)
		^ (static_cast<int32>(FMath::FloorToInt32(Location.Z)) * 7);

	return FPetalVariation::Hash01(LocationHash, 20261013, 5) * 2.0f * static_cast<float>(UE_PI);
}
