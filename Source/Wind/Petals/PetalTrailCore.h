// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PetalTypes.h"

/** One sampled point of the trail path. */
struct FPetalTrailNode
{
	FVector Position = FVector::ZeroVector;

	/** Monotonically increasing node index; -1 means this slot has never been written. */
	int64 Serial = -1;
};

/**
 * Trail backbone: records where the head has been, at a fixed spacing, into a ring buffer, and
 * maps trail petals onto those nodes.
 *
 * The point of the ring is that a trail petal is identified by its serial ("how far behind the
 * head"), not by "the m-th petal". The petal with serial s lives in slot s % Capacity, so when the
 * head advances the petals do not move to new slots: each slot simply holds one node, and the
 * renderer only rewrites the slots that actually changed. That keeps the per-frame cost of the
 * trail independent of how many petals it holds.
 *
 * This class is pure data plus arithmetic - no UObject, no world - so the whole trail model can be
 * covered by automation tests.
 */
class FPetalTrailCore
{
public:
	/** Smallest spacing the core will honour, in cm. */
	static constexpr float MinSpacing = 1.0f;

	/** (Re)builds the ring. Capacity is clamped to at least zero; spacing to at least MinSpacing. */
	void Initialize(int32 InCapacity, float InSpacing);

	/** Drops every node and every petal, then anchors the trail at StartPosition. */
	void Reset(const FVector& StartPosition);

	/** Drops every node and every petal, leaving the core unanchored. */
	void Clear();

	// --- Path -----------------------------------------------------------------

	void SetSpacing(float InSpacing);
	float GetSpacing() const { return Spacing; }

	/** Feeds the newest head position, recording as many evenly spaced nodes as the movement allows. */
	void PushHead(const FVector& NewPosition);

	/**
	 * Cap on nodes recorded by a single PushHead. A hitch or a teleport would otherwise rewrite the
	 * whole ring in one frame; when the cap is hit the rest of the segment is dropped.
	 */
	int32 MaxNodesPerPush = 128;

	// --- Petals ---------------------------------------------------------------

	int32 GetCapacity() const { return Capacity; }
	int32 GetPetalCount() const { return PetalCount; }

	/** Clamps into [0, Capacity]. Newly included / excluded slots become dirty. */
	void SetPetalCount(int32 InCount);

	/** Returns the new petal count. Negative input is ignored. */
	int32 AddPetals(int32 Count);

	/** Returns the new petal count. Returns how many were actually released through the out param. */
	int32 ReleasePetals(int32 Count, int32& OutReleasedCount);

	// --- Renderer hand-off ----------------------------------------------------

	/**
	 * Slots whose instance transform must be rewritten this frame, de-duplicated. A slot only tells
	 * the renderer "something changed here"; whether it should now be visible is answered by
	 * IsSlotVisible / GetSlotPosition.
	 */
	const TArray<int32>& GetDirtySlots() const { return DirtySlots; }
	void ClearDirtySlots();

	// --- Queries --------------------------------------------------------------

	/** Serial of the newest recorded node, or -1 when nothing has been recorded. */
	int64 GetHeadSerial() const { return HeadSerial; }

	/** How many slots currently hold a live node (at most Capacity). */
	int32 GetNumValidSlots() const { return NumValidSlots; }

	/** True when the slot holds a node that is inside the displayed petal window. */
	bool IsSlotVisible(int32 Slot) const;

	/** Position of the node in a slot. False when the slot holds no live node. */
	bool GetSlotPosition(int32 Slot, FVector& OutPosition) const;

	/** Direction along the trail at a slot, used to orient the petal. False when there is no node. */
	bool GetSlotTangent(int32 Slot, FVector& OutTangent) const;

private:
	static int32 SerialToSlot(int64 Serial, int32 InCapacity);

	bool IsSerialInRing(int64 Serial) const;
	bool IsSerialVisible(int64 Serial) const;
	bool GetSerialPosition(int64 Serial, FVector& OutPosition) const;

	void MarkSlotDirty(int64 Serial);
	void MarkDirtyRange(int64 FirstSerial, int64 LastSerial);
	void PushNode(const FVector& Position);

	int32 Capacity = 0;
	float Spacing = 15.0f;

	TArray<FPetalTrailNode> Nodes;
	TArray<int32> DirtySlots;
	TArray<uint8> DirtyMask;

	int64 HeadSerial = -1;
	int32 NumValidSlots = 0;
	int32 PetalCount = 0;

	bool bHasHead = false;
	FVector LastHeadPosition = FVector::ZeroVector;

	/** Distance travelled since the last recorded node, carried across frames. */
	float DistanceCarry = 0.0f;
};
