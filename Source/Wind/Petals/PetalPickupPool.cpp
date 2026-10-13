// Copyright Epic Games, Inc. All Rights Reserved.

#include "PetalPickupPool.h"

void FPetalPickupPool::Reset()
{
	Pickups.Reset();
	Grid.Reset();
	PendingRespawnCount = 0;
}

void FPetalPickupPool::Reserve(int32 Count)
{
	if (Count > 0)
	{
		Pickups.Reserve(Pickups.Num() + Count);
	}
}

int32 FPetalPickupPool::AddPickup(const FPetalPickup& InPickup)
{
	// Growing in chunks keeps the array from reallocating on every single petal, and because
	// entries are only ever appended, an index handed out earlier stays valid.
	if (Pickups.Num() == Pickups.Max())
	{
		Pickups.Reserve(Pickups.Num() + GrowthChunk);
	}

	const int32 Index = Pickups.Add(InPickup);

	FPetalPickup& Added = Pickups[Index];
	Added.Position = InPickup.HomePosition;
	Added.Velocity = FVector::ZeroVector;
	Added.bAlive = true;
	Added.bAttracted = false;
	Added.RespawnCountdown = -1.0f;

	// A pooled petal is part of the world until something pulls it in, so it goes straight into
	// the spatial index.
	AddToGrid(Index);

	return Index;
}

bool FPetalPickupPool::IsAlive(int32 Index) const
{
	return Pickups.IsValidIndex(Index) && Pickups[Index].bAlive;
}

int32 FPetalPickupPool::CountAlive() const
{
	int32 Count = 0;
	for (const FPetalPickup& Pickup : Pickups)
	{
		Count += Pickup.bAlive ? 1 : 0;
	}

	return Count;
}

int32 FPetalPickupPool::CountPendingRespawn() const
{
	// Counted as petals die and come back rather than scanned, because the field asks for this on
	// every tick simply to decide whether it needs to tick at all.
	return PendingRespawnCount;
}

void FPetalPickupPool::SetCellSize(float InCellSize)
{
	const float NewCellSize = FMath::Max(InCellSize, 1.0f);
	if (FMath::IsNearlyEqual(NewCellSize, CellSize))
	{
		return;
	}

	CellSize = NewCellSize;
	RebuildGrid();
}

void FPetalPickupPool::RebuildGrid()
{
	Grid.Reset();

	for (int32 Index = 0; Index < Pickups.Num(); ++Index)
	{
		if (Pickups[Index].bAlive && !Pickups[Index].bAttracted)
		{
			AddToGrid(Index);
		}
	}
}

void FPetalPickupPool::RemoveFromGrid(int32 Index)
{
	if (!Pickups.IsValidIndex(Index))
	{
		return;
	}

	// The cell is derived from the position rather than cached: every caller removes the petal
	// while it is still standing where it was indexed, so there is no state to keep in sync.
	const FIntVector Cell = WorldToCell(Pickups[Index].Position);
	TArray<int32>* Bucket = Grid.Find(Cell);
	if (!Bucket)
	{
		return;
	}

	Bucket->RemoveSingle(Index);
	if (Bucket->Num() == 0)
	{
		Grid.Remove(Cell);
	}
}

void FPetalPickupPool::AddToGrid(int32 Index)
{
	if (!Pickups.IsValidIndex(Index))
	{
		return;
	}

	Grid.FindOrAdd(WorldToCell(Pickups[Index].Position)).Add(Index);
}

void FPetalPickupPool::GatherNearby(const FVector& Center, float Radius, TArray<int32>& OutCandidates) const
{
	if (Radius <= 0.0f || Grid.Num() == 0)
	{
		return;
	}

	const int32 Range = FMath::Max(FMath::CeilToInt(Radius / CellSize), 0);
	const FIntVector CenterCell = WorldToCell(Center);
	const float RadiusSq = Radius * Radius;

	for (int32 Z = -Range; Z <= Range; ++Z)
	{
		for (int32 Y = -Range; Y <= Range; ++Y)
		{
			for (int32 X = -Range; X <= Range; ++X)
			{
				const FIntVector Cell(CenterCell.X + X, CenterCell.Y + Y, CenterCell.Z + Z);
				const TArray<int32>* Bucket = Grid.Find(Cell);
				if (!Bucket)
				{
					continue;
				}

				for (const int32 Index : *Bucket)
				{
					if (!Pickups.IsValidIndex(Index) || !Pickups[Index].bAlive || Pickups[Index].bAttracted)
					{
						continue;
					}

					if (FVector::DistSquared(Pickups[Index].Position, Center) <= RadiusSq)
					{
						OutCandidates.Add(Index);
					}
				}
			}
		}
	}
}

void FPetalPickupPool::SetAttracted(int32 Index, bool bAttracted)
{
	if (!Pickups.IsValidIndex(Index))
	{
		return;
	}

	FPetalPickup& Pickup = Pickups[Index];
	if (Pickup.bAttracted == bAttracted)
	{
		return;
	}

	Pickup.bAttracted = bAttracted;
	Pickup.Velocity = FVector::ZeroVector;

	if (bAttracted)
	{
		// Attracted petals are tracked by the field's attraction list, so they leave the grid.
		RemoveFromGrid(Index);
	}
	else if (Pickup.bAlive)
	{
		AddToGrid(Index);
	}
}

int32 FPetalPickupPool::CountAttracted() const
{
	int32 Count = 0;
	for (const FPetalPickup& Pickup : Pickups)
	{
		Count += Pickup.bAttracted ? 1 : 0;
	}

	return Count;
}

bool FPetalPickupPool::StepAttracted(int32 Index, const FVector& Target, float DeltaTime, float PullAcceleration, float Drag, float MaxSpeed)
{
	if (!Pickups.IsValidIndex(Index) || !Pickups[Index].bAttracted)
	{
		return false;
	}

	if (DeltaTime <= 0.0f)
	{
		return true;
	}

	FPetalPickup& Pickup = Pickups[Index];

	// The petal is dragged toward the magnet rather than snapped to it, so it accelerates in and
	// keeps a little of that speed on the way - the same layering used by the wind gust.
	const FVector ToTarget = Target - Pickup.Position;
	const FVector Direction = ToTarget.GetSafeNormal();

	Pickup.Velocity += Direction * FMath::Max(PullAcceleration, 0.0f) * DeltaTime;

	const float Damping = FMath::Clamp(1.0f - FMath::Max(Drag, 0.0f) * DeltaTime, 0.0f, 1.0f);
	Pickup.Velocity *= Damping;

	const float SpeedLimit = FMath::Max(MaxSpeed, 0.0f);
	if (SpeedLimit > 0.0f && Pickup.Velocity.SizeSquared() > SpeedLimit * SpeedLimit)
	{
		Pickup.Velocity = Pickup.Velocity.GetSafeNormal() * SpeedLimit;
	}

	Pickup.Position += Pickup.Velocity * DeltaTime;

	return true;
}

bool FPetalPickupPool::HasReachedTarget(int32 Index, const FVector& Target, float AbsorbRadius) const
{
	if (!Pickups.IsValidIndex(Index))
	{
		return false;
	}

	const float Radius = FMath::Max(AbsorbRadius, 0.0f);
	return FVector::DistSquared(Pickups[Index].Position, Target) <= Radius * Radius;
}

void FPetalPickupPool::Absorb(int32 Index, float RespawnDelay)
{
	if (!Pickups.IsValidIndex(Index))
	{
		return;
	}

	FPetalPickup& Pickup = Pickups[Index];

	RemoveFromGrid(Index);

	Pickup.bAlive = false;
	Pickup.bAttracted = false;
	Pickup.Velocity = FVector::ZeroVector;
	Pickup.Position = Pickup.HomePosition;
	Pickup.RespawnDelay = RespawnDelay;
	Pickup.RespawnCountdown = RespawnDelay;

	if (RespawnDelay >= 0.0f)
	{
		++PendingRespawnCount;
	}
}

int32 FPetalPickupPool::TickRespawn(float DeltaTime, TArray<int32>& OutRespawned)
{
	if (DeltaTime <= 0.0f)
	{
		return 0;
	}

	int32 Respawned = 0;

	for (int32 Index = 0; Index < Pickups.Num(); ++Index)
	{
		FPetalPickup& Pickup = Pickups[Index];
		if (Pickup.bAlive || Pickup.RespawnDelay < 0.0f)
		{
			continue;
		}

		Pickup.RespawnCountdown -= DeltaTime;
		if (Pickup.RespawnCountdown > 0.0f)
		{
			continue;
		}

		Pickup.bAlive = true;
		Pickup.bAttracted = false;
		Pickup.Position = Pickup.HomePosition;
		Pickup.Velocity = FVector::ZeroVector;
		Pickup.RespawnCountdown = -1.0f;

		PendingRespawnCount = FMath::Max(PendingRespawnCount - 1, 0);

		AddToGrid(Index);

		OutRespawned.Add(Index);
		++Respawned;
	}

	return Respawned;
}

void FPetalPickupPool::GetTransformsByGroup(FName Group, TArray<FTransform>& OutTransforms) const
{
	for (const FPetalPickup& Pickup : Pickups)
	{
		if (!Pickup.bAlive || Pickup.Group != Group)
		{
			continue;
		}

		OutTransforms.Add(FTransform(FRotator::ZeroRotator, Pickup.HomePosition, FVector(Pickup.Scale)));
	}
}

FIntVector FPetalPickupPool::WorldToCell(const FVector& Position) const
{
	return FIntVector(
		FMath::FloorToInt32(Position.X / CellSize),
		FMath::FloorToInt32(Position.Y / CellSize),
		FMath::FloorToInt32(Position.Z / CellSize));
}
