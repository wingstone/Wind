// Copyright Epic Games, Inc. All Rights Reserved.

#include "PetalMagnetComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "PetalPickupFieldComponent.h"
#include "PetalTrailComponent.h"
#include "Wind.h"

namespace
{
	/** Registered magnets, so placed hero petals can find their collector without a world scan. */
	TArray<TWeakObjectPtr<UPetalMagnetComponent>>& GetRegisteredMagnets()
	{
		static TArray<TWeakObjectPtr<UPetalMagnetComponent>> Magnets;
		return Magnets;
	}
}

UPetalMagnetComponent::UPetalMagnetComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	SetMobility(EComponentMobility::Movable);
}

void UPetalMagnetComponent::OnRegister()
{
	Super::OnRegister();

	GetRegisteredMagnets().AddUnique(this);
}

void UPetalMagnetComponent::OnUnregister()
{
	GetRegisteredMagnets().Remove(this);

	Super::OnUnregister();
}

void UPetalMagnetComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!Trail)
	{
		if (AActor* Owner = GetOwner())
		{
			Trail = Owner->FindComponentByClass<UPetalTrailComponent>();
		}
	}

	ensureMsgf(Trail != nullptr, TEXT("Petal magnet '%s' on '%s' has no petal trail to fill; collected petals would be discarded."),
		*GetName(), *GetNameSafe(GetOwner()));
}

void UPetalMagnetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void UPetalMagnetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnableMagnet)
	{
		return;
	}

	int32 Budget = FMath::Max(MaxAttractPerFrame, 0);
	if (Budget <= 0)
	{
		return;
	}

	const FVector Origin = GetMagnetLocation();

	UPetalPickupFieldComponent::ForEachRegisteredField(GetWorld(), [&](UPetalPickupFieldComponent* Field)
	{
		if (Field && Budget > 0)
		{
			Budget -= Field->AttractPickupsInRange(Origin, MagnetRadius, this, Budget);
		}
	});

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), Origin, MagnetRadius, 24, FColor::Cyan, false, -1.0f, 0, 1.0f);
		DrawDebugSphere(GetWorld(), Origin, AbsorbRadius, 12, FColor::Green, false, -1.0f, 0, 2.0f);
	}
#endif
}

FVector UPetalMagnetComponent::GetMagnetLocation() const
{
	if (SourceComponent && SourceComponent->IsRegistered())
	{
		return SourceComponent->GetComponentLocation();
	}

	if (const AActor* Owner = GetOwner())
	{
		return Owner->GetActorLocation();
	}

	return GetComponentLocation();
}

void UPetalMagnetComponent::NotifyPickupAbsorbed(const FVector& Location, int32 PetalValue)
{
	if (Trail)
	{
		Trail->AddPetals(PetalValue);
	}

	OnPetalCollected.Broadcast(Location, PetalValue);
}

void UPetalMagnetComponent::ForEachRegisteredMagnet(const UWorld* World, TFunctionRef<void(UPetalMagnetComponent*)> Visitor)
{
	TArray<TWeakObjectPtr<UPetalMagnetComponent>>& Magnets = GetRegisteredMagnets();

	// Walk backwards so stale entries can be dropped while visiting.
	for (int32 Index = Magnets.Num() - 1; Index >= 0; --Index)
	{
		UPetalMagnetComponent* Magnet = Magnets[Index].Get();
		if (!Magnet || (World && Magnet->GetWorld() != World))
		{
			if (!Magnet)
			{
				Magnets.RemoveAtSwap(Index);
			}

			continue;
		}

		Visitor(Magnet);
	}
}

UPetalMagnetComponent* UPetalMagnetComponent::FindNearestMagnet(const UWorld* World, const FVector& Location, float& OutDistanceSquared)
{
	UPetalMagnetComponent* Nearest = nullptr;
	float NearestDistanceSq = 0.0f;

	ForEachRegisteredMagnet(World, [&](UPetalMagnetComponent* Magnet)
	{
		const float DistanceSq = FVector::DistSquared(Magnet->GetMagnetLocation(), Location);
		if (!Nearest || DistanceSq < NearestDistanceSq)
		{
			Nearest = Magnet;
			NearestDistanceSq = DistanceSq;
		}
	});

	OutDistanceSquared = NearestDistanceSq;
	return Nearest;
}
