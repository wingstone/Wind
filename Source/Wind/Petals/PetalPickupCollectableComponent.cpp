// Copyright Epic Games, Inc. All Rights Reserved.

#include "PetalPickupCollectableComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "PetalMagnetComponent.h"
#include "Wind.h"

UPetalPickupCollectableComponent::UPetalPickupCollectableComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	SetMobility(EComponentMobility::Movable);
}

void UPetalPickupCollectableComponent::BeginPlay()
{
	Super::BeginPlay();

	SetComponentTickEnabled(bEnable);
}

void UPetalPickupCollectableComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	float DistanceSq = 0.0f;
	UPetalMagnetComponent* Magnet = UPetalMagnetComponent::FindNearestMagnet(GetWorld(), Owner->GetActorLocation(), DistanceSq);
	if (!Magnet)
	{
		return;
	}

	const FVector Target = Magnet->GetMagnetLocation();

	if (DistanceSq <= FMath::Square(Magnet->GetAbsorbRadius()))
	{
		Collect(*Magnet);
		return;
	}

	if (DistanceSq > FMath::Square(Magnet->GetMagnetRadius()))
	{
		return;
	}

	// Inside the radius the petal glides in, so a hero petal responds the same way a pooled one does.
	const FVector Position = Owner->GetActorLocation();
	const FVector Direction = (Target - Position).GetSafeNormal();
	Owner->SetActorLocation(Position + Direction * FMath::Max(ApproachSpeed, 0.0f) * DeltaTime);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugLine(GetWorld(), Position, Target, FColor::Cyan, false, -1.0f, 0, 1.0f);
	}
#endif
}

void UPetalPickupCollectableComponent::Collect(UPetalMagnetComponent& Magnet)
{
	AActor* Owner = GetOwner();
	const FVector Location = Owner ? Owner->GetActorLocation() : GetComponentLocation();

	Magnet.NotifyPickupAbsorbed(Location, FMath::Max(PetalValue, 0));

	if (bDestroyAfterCollect)
	{
		if (Owner)
		{
			Owner->Destroy();
		}
	}
	else
	{
		SetComponentTickEnabled(false);
	}
}
