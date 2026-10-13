// Copyright Epic Games, Inc. All Rights Reserved.

#include "PetalTrailComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Wind.h"

UPetalTrailComponent::UPetalTrailComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	SetMobility(EComponentMobility::Movable);

	PetalInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PetalInstances"));
	PetalInstances->SetupAttachment(this);
	PetalInstances->SetMobility(EComponentMobility::Movable);
	PetalInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PetalInstances->SetGenerateOverlapEvents(false);
	PetalInstances->SetCastShadow(false);
	PetalInstances->SetCanEverAffectNavigation(false);
	PetalInstances->bReceivesDecals = false;
	PetalInstances->bUseAsOccluder = false;
	PetalInstances->SetNumCustomDataFloats(2);

	// A plane keeps the system asset-free until a real petal mesh is assigned.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultPetalMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (DefaultPetalMesh.Succeeded())
	{
		PetalMesh = DefaultPetalMesh.Object;
		PetalInstances->SetStaticMesh(PetalMesh);
	}
}

void UPetalTrailComponent::OnRegister()
{
	Super::OnRegister();

	// Instance transforms are stored in world space on purpose. Going absolute keeps the anchor
	// fixed no matter what the component is attached to, because otherwise every instance would
	// have to be rewritten the moment the parent moved, which is exactly what this design avoids.
	SetAbsolute(true, true, true);

	if (PetalInstances)
	{
		if (PetalMesh)
		{
			PetalInstances->SetStaticMesh(PetalMesh);
		}

		if (PetalMaterial)
		{
			PetalInstances->SetMaterial(0, PetalMaterial);
		}

		PetalInstances->SetCullDistances(0, FMath::RoundToInt(FMath::Max(InstanceCullDistance, 0.0f)));
	}
}

void UPetalTrailComponent::BeginPlay()
{
	Super::BeginPlay();

	EnsureInstances();
}

void UPetalTrailComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	EnsureInstances();

	Core.PushHead(ResolveHeadLocation());
	FlushDirtySlots();

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		const int32 NumSlots = Core.GetCapacity();
		for (int32 Slot = 0; Slot < NumSlots; ++Slot)
		{
			FVector Position;
			if (Core.IsSlotVisible(Slot) && Core.GetSlotPosition(Slot, Position))
			{
				DrawDebugPoint(GetWorld(), Position, 6.0f, FColor::Magenta, false, -1.0f, 0);
			}
		}

		if (const AActor* Owner = GetOwner())
		{
			DrawDebugString(GetWorld(), FVector(0.0f, 0.0f, 120.0f), FString::Printf(TEXT("Petals %d / %d"), Core.GetPetalCount(), Core.GetCapacity()), const_cast<AActor*>(Owner), FColor::Magenta, 0.0f);
		}
	}
#endif
}

int32 UPetalTrailComponent::AddPetals(int32 Count)
{
	EnsureInstances();

	const int32 Previous = Core.GetPetalCount();
	const int32 NewCount = Core.AddPetals(Count);

	if (NewCount != Previous)
	{
		OnPetalCountChanged.Broadcast(NewCount);
	}

	return NewCount;
}

int32 UPetalTrailComponent::ReleasePetals(int32 Count)
{
	int32 Released = 0;
	const int32 NewCount = Core.ReleasePetals(Count, Released);

	if (Released > 0)
	{
		FlushDirtySlots();
		OnPetalsReleased.Broadcast(Released);
		OnPetalCountChanged.Broadcast(NewCount);
	}

	return NewCount;
}

void UPetalTrailComponent::SetPetalCount(int32 Count)
{
	EnsureInstances();

	const int32 Previous = Core.GetPetalCount();
	Core.SetPetalCount(Count);

	if (Core.GetPetalCount() != Previous)
	{
		FlushDirtySlots();
		OnPetalCountChanged.Broadcast(Core.GetPetalCount());
	}
}

int32 UPetalTrailComponent::GetPetalCount() const
{
	return Core.GetPetalCount();
}

void UPetalTrailComponent::ClearTrail()
{
	const int32 Previous = Core.GetPetalCount();
	Core.Clear();
	Core.ClearDirtySlots();

	if (PetalInstances)
	{
		const int32 NumInstances = PetalInstances->GetInstanceCount();
		for (int32 Index = 0; Index < NumInstances; ++Index)
		{
			PetalInstances->UpdateInstanceTransform(Index, FPetalVariation::MakeHiddenTransform(), true, false, true);
		}

		if (NumInstances > 0)
		{
			PetalInstances->MarkRenderStateDirty();
		}
	}

	if (Previous != 0)
	{
		OnPetalCountChanged.Broadcast(0);
	}
}

void UPetalTrailComponent::EnsureInstances()
{
	if (bInstancesReady || !PetalInstances)
	{
		return;
	}

	const int32 Capacity = FMath::Max(MaxPetals, 1);
	bInstancesReady = true;

	Core.Initialize(Capacity, PetalSpacing);
	Core.Reset(ResolveHeadLocation());

	SlotScale.SetNum(Capacity);
	SlotRoll.SetNum(Capacity);
	SlotTilt.SetNum(Capacity);

	for (int32 Index = 0; Index < Capacity; ++Index)
	{
		SlotScale[Index] = FPetalVariation::MakeScale(Index, RandomSeed, PetalScale, PetalScaleJitter);
		SlotRoll[Index] = FPetalVariation::MakeRollDegrees(Index, RandomSeed);
		SlotTilt[Index] = FPetalVariation::MakeTiltDegrees(Index, RandomSeed, PetalTiltDegrees);
	}

	if (PetalMesh)
	{
		PetalInstances->SetStaticMesh(PetalMesh);
	}

	if (PetalMaterial)
	{
		PetalInstances->SetMaterial(0, PetalMaterial);
	}

	// Every instance is created up front and parked at a hidden scale, so collecting a petal costs
	// one instance write instead of an AddInstance / RemoveInstance round trip.
	PetalInstances->PreAllocateInstancesMemory(Capacity);

	TArray<FTransform> HiddenTransforms;
	HiddenTransforms.Init(FPetalVariation::MakeHiddenTransform(), Capacity);
	PetalInstances->AddInstances(HiddenTransforms, false, /*bWorldSpace=*/true);

	// Per-slot variation never changes while the slot exists, so it is written once.
	for (int32 Index = 0; Index < Capacity; ++Index)
	{
		PetalInstances->SetCustomDataValue(Index, 0, FPetalVariation::MakePhase(Index, RandomSeed), false);
		PetalInstances->SetCustomDataValue(Index, 1, FPetalVariation::MakeVariationIndex(Index, RandomSeed, 4), false);
	}

	PetalInstances->MarkRenderStateDirty();

	// The hidden pass above already covers every slot.
	Core.ClearDirtySlots();

	UE_LOG(LogWind, Log, TEXT("Petal trail '%s' ready: %d petal slots at %.1f cm spacing (%.1f m of trail)."),
		*GetNameSafe(this), Capacity, Core.GetSpacing(), Capacity * Core.GetSpacing() / 100.0f);
}

void UPetalTrailComponent::FlushDirtySlots()
{
	if (!PetalInstances)
	{
		Core.ClearDirtySlots();
		return;
	}

	const TArray<int32>& DirtySlots = Core.GetDirtySlots();
	if (DirtySlots.Num() == 0)
	{
		return;
	}

	FVector Position;
	FVector Tangent;

	for (const int32 Slot : DirtySlots)
	{
		if (!PetalInstances->IsValidInstance(Slot))
		{
			continue;
		}

		if (Core.GetSlotPosition(Slot, Position) && Core.IsSlotVisible(Slot))
		{
			Core.GetSlotTangent(Slot, Tangent);
			PetalInstances->UpdateInstanceTransform(Slot, BuildPetalTransform(Slot, Position, Tangent), /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/false, /*bTeleport=*/true);
		}
		else
		{
			PetalInstances->UpdateInstanceTransform(Slot, FPetalVariation::MakeHiddenTransform(), true, false, true);
		}
	}

	// One dirty mark for the whole batch: the instanced mesh uploads only the instances that
	// actually changed, which is why updating a handful of slots per frame stays cheap.
	PetalInstances->MarkRenderStateDirty();
	Core.ClearDirtySlots();
}

FVector UPetalTrailComponent::ResolveHeadLocation() const
{
	if (FollowComponent && FollowComponent->IsRegistered())
	{
		return FollowComponent->GetComponentLocation();
	}

	if (const AActor* Owner = GetOwner())
	{
		return Owner->GetActorLocation();
	}

	return GetComponentLocation();
}

FTransform UPetalTrailComponent::BuildPetalTransform(int32 Slot, const FVector& Position, const FVector& Tangent) const
{
	const FVector Direction = Tangent.IsNearlyZero() ? FVector::ForwardVector : Tangent.GetSafeNormal();
	const FQuat Facing = Direction.ToOrientationQuat();

	const float Roll = SlotRoll.IsValidIndex(Slot) ? SlotRoll[Slot] : 0.0f;
	const float Tilt = SlotTilt.IsValidIndex(Slot) ? SlotTilt[Slot] : 0.0f;
	const float Scale = SlotScale.IsValidIndex(Slot) ? SlotScale[Slot] : PetalScale;

	// Roll around the trail, then tilt off it, so a plain plane already reads as a loose petal.
	const FQuat Orientation = Facing
		* FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Roll))
		* FQuat(FVector::UpVector, FMath::DegreesToRadians(Tilt));

	return FTransform(Orientation, Position, FVector(Scale));
}
