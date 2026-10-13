// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PetalTypes.h"

/**
 * Every pending petal in one field, as plain data.
 *
 * The pool never hands out actors or components: a petal is an entry in one array plus one instance
 * of the field's instanced mesh. Entries are appended and reused in place, so an index stays valid
 * for the lifetime of the field and the renderer can address an instance by the same index.
 *
 * The uniform grid keeps the magnet query proportional to what is actually near the player instead
 * of to how many petals the field holds. Petals that are being pulled in are removed from the grid,
 * because they are tracked by the field's attraction list instead.
 *
 * Pure data plus arithmetic - no UObject, no world - so it can be covered by automation tests.
 */
class FPetalPickupPool
{
public:
	/** How many slots are reserved at a time when the pool has to grow. */
	static constexpr int32 GrowthChunk = 256;

	void Reset();

	/** Reserves room without creating petal entries. */
	void Reserve(int32 Count);

	/** Appends one petal and returns its index. Existing indices never move. */
	int32 AddPickup(const FPetalPickup& InPickup);

	int32 Num() const { return Pickups.Num(); }
	bool IsValidIndex(int32 Index) const { return Pickups.IsValidIndex(Index); }

	const FPetalPickup& GetPickup(int32 Index) const { return Pickups[Index]; }
	FPetalPickup& GetPickupMutable(int32 Index) { return Pickups[Index]; }

	bool IsAlive(int32 Index) const;
	int32 CountAlive() const;

	/** Petals waiting on their respawn timer; the field only needs to tick while this is non-zero. */
	int32 CountPendingRespawn() const;

	// --- Spatial index --------------------------------------------------------

	void SetCellSize(float InCellSize);
	float GetCellSize() const { return CellSize; }

	/** Indexes every live petal from scratch. Call once the pool has been filled. */
	void RebuildGrid();

	void RemoveFromGrid(int32 Index);
	void AddToGrid(int32 Index);

	/** Collects live, non-attracted petal indices within Radius of Center. */
	void GatherNearby(const FVector& Center, float Radius, TArray<int32>& OutCandidates) const;

	// --- Attraction and collection --------------------------------------------

	/** Flags a petal as being pulled in (removing it from the grid) or returns it to the world. */
	void SetAttracted(int32 Index, bool bAttracted);

	int32 CountAttracted() const;

	/** Integrates one attracted petal toward Target. False when the index is not attracted. */
	bool StepAttracted(int32 Index, const FVector& Target, float DeltaTime, float PullAcceleration, float Drag, float MaxSpeed);

	bool HasReachedTarget(int32 Index, const FVector& Target, float AbsorbRadius) const;

	/** Marks a petal collected. A negative RespawnDelay leaves it dead for good. */
	void Absorb(int32 Index, float RespawnDelay);

	/** Ticks respawn timers. Appends the indices that came back this step to OutRespawned. */
	int32 TickRespawn(float DeltaTime, TArray<int32>& OutRespawned);

	/** Authored transforms (home positions) of every alive petal in a group. */
	void GetTransformsByGroup(FName Group, TArray<FTransform>& OutTransforms) const;

private:
	FIntVector WorldToCell(const FVector& Position) const;

	TArray<FPetalPickup> Pickups;
	TMap<FIntVector, TArray<int32>> Grid;

	/** Petals waiting on a respawn timer, maintained as they die and come back. */
	int32 PendingRespawnCount = 0;

	/** Grid cell size, in cm. Normally the magnet radius, so a query only touches 27 cells. */
	float CellSize = 1500.0f;
};
