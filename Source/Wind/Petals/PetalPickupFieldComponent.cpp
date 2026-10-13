// Copyright Epic Games, Inc. All Rights Reserved.

#include "PetalPickupFieldComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "PetalMagnetComponent.h"
#include "PetalPickup.h"
#include "UObject/ConstructorHelpers.h"
#include "Wind.h"

namespace
{
	/** How many captured actors are destroyed per frame, so level start stays smooth. */
	constexpr int32 DestroyBatchPerFrame = 8;

	/** Registered fields, so magnets and placed petals can find them without a world scan. */
	TArray<TWeakObjectPtr<UPetalPickupFieldComponent>>& GetRegisteredFields()
	{
		static TArray<TWeakObjectPtr<UPetalPickupFieldComponent>> Fields;
		return Fields;
	}
}

UPetalPickupFieldComponent::UPetalPickupFieldComponent()
{
	// A field idles: it is switched on only while petals are actually flying or respawning.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	SetMobility(EComponentMobility::Movable);

	PickupInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PickupInstances"));
	PickupInstances->SetupAttachment(this);
	PickupInstances->SetMobility(EComponentMobility::Movable);
	PickupInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupInstances->SetGenerateOverlapEvents(false);
	PickupInstances->SetCastShadow(false);
	PickupInstances->SetCanEverAffectNavigation(false);
	PickupInstances->bReceivesDecals = false;
	PickupInstances->bUseAsOccluder = false;
	PickupInstances->SetNumCustomDataFloats(2);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultPickupMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (DefaultPickupMesh.Succeeded())
	{
		PickupMesh = DefaultPickupMesh.Object;
		PickupInstances->SetStaticMesh(PickupMesh);
	}
}

void UPetalPickupFieldComponent::OnRegister()
{
	Super::OnRegister();

	// Registered here rather than in BeginPlay so that placed petals, whose BeginPlay may run first,
	// always find a field to join.
	GetRegisteredFields().AddUnique(this);

	// Petals are stored in world space, so the field must not drag them around when its parent moves.
	SetAbsolute(true, true, true);

	if (PickupInstances)
	{
		if (PickupMesh)
		{
			PickupInstances->SetStaticMesh(PickupMesh);
		}

		if (PickupMaterial)
		{
			PickupInstances->SetMaterial(0, PickupMaterial);
		}

		PickupInstances->SetCullDistances(0, FMath::RoundToInt(FMath::Max(InstanceCullDistance, 0.0f)));
	}
}

void UPetalPickupFieldComponent::OnUnregister()
{
	GetRegisteredFields().Remove(this);

	Super::OnUnregister();
}

void UPetalPickupFieldComponent::BeginPlay()
{
	Super::BeginPlay();

	if (PickupMesh)
	{
		PickupInstances->SetStaticMesh(PickupMesh);
	}

	if (PickupMaterial)
	{
		PickupInstances->SetMaterial(0, PickupMaterial);
	}

	// The cell size only affects query cost, never correctness, so it is set before anything is indexed.
	Pool.SetCellSize(GridCellSize);

	if (bCapturePlacedPickups)
	{
		const int32 Captured = CapturePlacedPickups();
		if (Captured > 0)
		{
			UE_LOG(LogWind, Log, TEXT("Petal field '%s' took over %d placed petals."), *GetNameSafe(this), Captured);
		}
	}

	SeedProceduralPickups();

	UE_LOG(LogWind, Log, TEXT("Petal field '%s' holds %d petals (%d of them placed by hand)."),
		*GetNameSafe(this), Pool.Num(), CapturedPlacedCount);

	UpdateTickEnabled();
}

void UPetalPickupFieldComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetRegisteredFields().Remove(this);

	Super::EndPlay(EndPlayReason);
}

void UPetalPickupFieldComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	int32 InstanceWrites = 0;

	// Captured actors are destroyed a few per frame rather than all in the frame they are taken over.
	if (PendingDestroy.Num() > 0)
	{
		const int32 Batch = FMath::Min(PendingDestroy.Num(), DestroyBatchPerFrame);
		for (int32 Index = Batch - 1; Index >= 0; --Index)
		{
			if (APetalPickup* PlacedPetal = PendingDestroy[Index].Get())
			{
				PlacedPetal->Destroy();
			}

			PendingDestroy.RemoveAtSwap(Index);
		}
	}

	// Integrate only the petals that are actually being pulled in - everything else in the field is
	// standing still on purpose.
	for (int32 AttractionIndex = Attractions.Num() - 1; AttractionIndex >= 0; --AttractionIndex)
	{
		UPetalMagnetComponent* Magnet = Attractions[AttractionIndex].Magnet.Get();
		const int32 PickupIndex = Attractions[AttractionIndex].PickupIndex;

		if (!Magnet || !Pool.IsValidIndex(PickupIndex) || !Pool.GetPickup(PickupIndex).bAttracted)
		{
			Attractions.RemoveAtSwap(AttractionIndex);
			continue;
		}

		const FVector Target = Magnet->GetMagnetLocation();
		Pool.StepAttracted(PickupIndex, Target, DeltaTime, PullAcceleration, AttractDrag, MaxAttractSpeed);
		WritePickupInstance(PickupIndex);
		++InstanceWrites;

		if (Pool.HasReachedTarget(PickupIndex, Target, Magnet->GetAbsorbRadius()))
		{
			// Read what the petal is worth before absorbing it.
			const FPetalPickup& Collected = Pool.GetPickup(PickupIndex);
			const FVector Location = Collected.Position;
			const int32 Value = FMath::Max(Collected.Value, 0);
			const float RespawnDelay = Collected.RespawnDelay;

			Pool.Absorb(PickupIndex, RespawnDelay);
			ParkPickupInstance(PickupIndex);
			++InstanceWrites;

			Attractions.RemoveAtSwap(AttractionIndex);

			OnPickupAbsorbed.Broadcast(Location, Value);
			Magnet->NotifyPickupAbsorbed(Location, Value);
		}
	}

	if (Pool.TickRespawn(DeltaTime, RespawnScratch) > 0)
	{
		for (const int32 RespawnedIndex : RespawnScratch)
		{
			WritePickupInstance(RespawnedIndex);
			++InstanceWrites;
		}

		RespawnScratch.Reset();
	}

	if (InstanceWrites > 0)
	{
		// One dirty mark for the batch: only the instances whose data changed get uploaded.
		PickupInstances->MarkRenderStateDirty();
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugBox(GetWorld(), GetComponentLocation(), FieldExtents, FColor::Cyan, false, -1.0f, 0, 1.0f);
		DrawDebugString(GetWorld(), GetComponentLocation(), FString::Printf(TEXT("Petals %d alive / %d total, %d flying"), Pool.CountAlive(), Pool.Num(), Attractions.Num()), nullptr, FColor::Cyan, 0.0f);
	}
#endif

	UpdateTickEnabled();
}

int32 UPetalPickupFieldComponent::CapturePlacedPickups()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	int32 Captured = 0;

	for (TActorIterator<APetalPickup> It(World); It; ++It)
	{
		APetalPickup* PlacedPetal = *It;
		if (!PlacedPetal || PlacedPetal->IsCaptured() || !PlacedPetal->ShouldCaptureOnBeginPlay())
		{
			continue;
		}

		// With several fields around, only the closest one claims a petal, so placement decides.
		if (!IsNearestFieldTo(PlacedPetal))
		{
			continue;
		}

		Captured += CapturePickup(PlacedPetal);
	}

	return Captured;
}

int32 UPetalPickupFieldComponent::CapturePickup(APetalPickup* PlacedPetal)
{
	if (!PlacedPetal || PlacedPetal->IsCaptured() || PlacedPetal->IsAuthoringOnly())
	{
		return 0;
	}

	AddPickup(
		PlacedPetal->GetActorTransform(),
		PlacedPetal->GetPetalValue(),
		PlacedPetal->GetGroup(),
		PlacedPetal->GetPetalScale(),
		PlacedPetal->GetVariation(),
		PlacedPetal->GetPickupPhase(),
		PlacedPetal->GetAttractRadiusOverride(),
		PlacedPetal->GetPickupRespawnDelay());

	// The actor's job is done the moment its transform and parameters are in the pool.
	PlacedPetal->OnCaptured();

	if (bDestroyCapturedPickups)
	{
		PendingDestroy.Add(PlacedPetal);
	}

	++CapturedPlacedCount;
	UpdateTickEnabled();

	return 1;
}

bool UPetalPickupFieldComponent::IsNearestFieldTo(const AActor* Other) const
{
	if (!Other)
	{
		return false;
	}

	const FVector Location = Other->GetActorLocation();
	const float MyDistanceSq = FVector::DistSquared(GetComponentLocation(), Location);
	bool bNearest = true;

	ForEachRegisteredField(GetWorld(), [&](UPetalPickupFieldComponent* Field)
	{
		if (Field == this || !bNearest)
		{
			return;
		}

		if (FVector::DistSquared(Field->GetComponentLocation(), Location) < MyDistanceSq)
		{
			bNearest = false;
		}
	});

	return bNearest;
}

int32 UPetalPickupFieldComponent::AddPickup(const FTransform& Transform, int32 Value, FName Group, float Scale, float Variation, float Phase, float AttractRadiusOverride, float RespawnDelay)
{
	FPetalPickup NewPickup;
	NewPickup.HomePosition = Transform.GetLocation();
	NewPickup.Scale = FMath::Max(Scale, 0.0f);
	NewPickup.Variation = Variation;
	NewPickup.Phase = Phase;
	NewPickup.Value = FMath::Max(Value, 0);
	NewPickup.Group = Group;
	NewPickup.AttractRadiusOverride = FMath::Max(AttractRadiusOverride, 0.0f);
	NewPickup.RespawnDelay = RespawnDelay;

	const int32 Index = Pool.AddPickup(NewPickup);

	EnsureInstances(Pool.Num());
	WritePickupInstance(Index);

	return Index;
}

void UPetalPickupFieldComponent::EnsurePickupCapacity(int32 Count)
{
	const int32 Required = FMath::Max(Count, 0);

	Pool.Reserve(Required - Pool.Num());
	EnsureInstances(Required);
}

int32 UPetalPickupFieldComponent::AttractPickupsInRange(const FVector& Center, float Radius, UPetalMagnetComponent* Magnet, int32 MaxCount){
	if (!Magnet || MaxCount <= 0 || Radius <= 0.0f)
	{
		return 0;
	}

	// A magnet much larger than the configured bucket would make the neighbour walk expensive;
	// re-bucketing once is cheaper than querying thousands of cells every frame.
	if (Radius > Pool.GetCellSize() * 4.0f)
	{
		Pool.SetCellSize(Radius);
	}

	CandidateScratch.Reset();
	Pool.GatherNearby(Center, Radius, CandidateScratch);

	int32 Flagged = 0;

	for (const int32 Index : CandidateScratch)
	{
		if (Flagged >= MaxCount)
		{
			break;
		}

		const FPetalPickup& Pickup = Pool.GetPickup(Index);

		// A placed petal may override the radius, which is how a designer makes one stretch of the
		// path easier to sweep up than the rest.
		const float EffectiveRadius = Pickup.AttractRadiusOverride > 0.0f ? Pickup.AttractRadiusOverride : Radius;
		if (FVector::DistSquared(Pickup.Position, Center) > EffectiveRadius * EffectiveRadius)
		{
			continue;
		}

		Pool.SetAttracted(Index, true);

		FAttraction& Attraction = Attractions.AddDefaulted_GetRef();
		Attraction.PickupIndex = Index;
		Attraction.Magnet = Magnet;

		++Flagged;
	}

	UpdateTickEnabled();

	return Flagged;
}

void UPetalPickupFieldComponent::ForEachRegisteredField(const UWorld* World, TFunctionRef<void(UPetalPickupFieldComponent*)> Visitor)
{
	TArray<TWeakObjectPtr<UPetalPickupFieldComponent>>& Fields = GetRegisteredFields();

	// Walk backwards so stale entries can be dropped while visiting.
	for (int32 Index = Fields.Num() - 1; Index >= 0; --Index)
	{
		UPetalPickupFieldComponent* Field = Fields[Index].Get();
		if (!Field || (World && Field->GetWorld() != World))
		{
			if (!Field)
			{
				Fields.RemoveAtSwap(Index);
			}

			continue;
		}

		Visitor(Field);
	}
}

UPetalPickupFieldComponent* UPetalPickupFieldComponent::FindNearestField(const UWorld* World, const FVector& Location)
{
	UPetalPickupFieldComponent* Nearest = nullptr;
	float NearestDistanceSq = 0.0f;

	ForEachRegisteredField(World, [&](UPetalPickupFieldComponent* Field)
	{
		const float DistanceSq = FVector::DistSquared(Field->GetComponentLocation(), Location);
		if (!Nearest || DistanceSq < NearestDistanceSq)
		{
			Nearest = Field;
			NearestDistanceSq = DistanceSq;
		}
	});

	return Nearest;
}

void UPetalPickupFieldComponent::SeedProceduralPickups()
{
	const double ExtentX = FMath::Max(FieldExtents.X, 0.0);
	const double ExtentY = FMath::Max(FieldExtents.Y, 0.0);
	const double ExtentZ = FMath::Max(FieldExtents.Z, 0.0);

	const double VolumeM3 = (ExtentX * 2.0) * (ExtentY * 2.0) * (ExtentZ * 2.0) / 1000000.0;
	const int32 Cap = FMath::Max(MaxProceduralPickups, 0);
	const int32 Requested = FMath::Clamp(FMath::RoundToInt32(static_cast<double>(FMath::Max(Density, 0.0f)) * VolumeM3), 0, Cap);

	if (Requested <= 0)
	{
		return;
	}

	FRandomStream Random(RandomSeed);
	const FVector Center = GetComponentLocation();
	const float RespawnDelay = bRespawnProceduralPickups ? FMath::Max(DefaultRespawnDelay, 0.0f) : -1.0f;

	for (int32 Index = 0; Index < Requested; ++Index)
	{
		const FVector Offset(
			Random.FRandRange(-static_cast<float>(ExtentX), static_cast<float>(ExtentX)),
			Random.FRandRange(-static_cast<float>(ExtentY), static_cast<float>(ExtentY)),
			Random.FRandRange(-static_cast<float>(ExtentZ), static_cast<float>(ExtentZ)));

		const float Scale = FPetalVariation::MakeScale(Index, RandomSeed, PickupScale, PickupScaleJitter);
		const float Variation = FPetalVariation::MakeVariationIndex(Index, RandomSeed, FMath::Max(RandomVariationCount, 1));
		const float Phase = FPetalVariation::MakePhase(Index, RandomSeed);

		AddPickup(FTransform(FRotator::ZeroRotator, Center + Offset), 1, NAME_None, Scale, Variation, Phase, 0.0f, RespawnDelay);
	}
}

void UPetalPickupFieldComponent::EnsureInstances(int32 RequiredCount)
{
	if (!PickupInstances)
	{
		return;
	}

	const int32 Current = PickupInstances->GetInstanceCount();
	if (RequiredCount <= Current)
	{
		return;
	}

	// Slots are added in chunks and never removed, so an instance index stays valid for the life of
	// the field and the pool index can address it directly.
	const int32 AddedCount = FMath::Max(RequiredCount - Current, FPetalPickupPool::GrowthChunk);

	TArray<FTransform> HiddenTransforms;
	HiddenTransforms.Init(FPetalVariation::MakeHiddenTransform(), AddedCount);
	PickupInstances->AddInstances(HiddenTransforms, false, /*bWorldSpace=*/true);

	PickupInstances->MarkRenderStateDirty();
}

void UPetalPickupFieldComponent::WritePickupInstance(int32 Index)
{
	if (!PickupInstances || !Pool.IsValidIndex(Index) || !PickupInstances->IsValidInstance(Index))
	{
		return;
	}

	const FPetalPickup& Pickup = Pool.GetPickup(Index);
	if (!Pickup.bAlive || Pickup.Scale <= 0.0f)
	{
		ParkPickupInstance(Index);
		return;
	}

	const FRotator Orientation(0.0f, FPetalVariation::MakeRollDegrees(Index, RandomSeed), 0.0f);
	PickupInstances->UpdateInstanceTransform(Index, FTransform(Orientation, Pickup.Position, FVector(Pickup.Scale)), /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/false, /*bTeleport=*/true);

	PickupInstances->SetCustomDataValue(Index, 0, Pickup.Phase, false);
	PickupInstances->SetCustomDataValue(Index, 1, Pickup.Variation, false);
}

void UPetalPickupFieldComponent::ParkPickupInstance(int32 Index)
{
	if (!PickupInstances || !PickupInstances->IsValidInstance(Index))
	{
		return;
	}

	PickupInstances->UpdateInstanceTransform(Index, FPetalVariation::MakeHiddenTransform(), true, false, true);
}

void UPetalPickupFieldComponent::UpdateTickEnabled()
{
	const bool bNeedsTick = Attractions.Num() > 0 || PendingDestroy.Num() > 0 || Pool.CountPendingRespawn() > 0;
	SetComponentTickEnabled(bNeedsTick);
}
