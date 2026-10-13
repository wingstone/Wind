// Copyright Epic Games, Inc. All Rights Reserved.

#include "PetalTrailCore.h"

void FPetalTrailCore::Initialize(int32 InCapacity, float InSpacing)
{
	Capacity = FMath::Max(InCapacity, 0);
	Spacing = FMath::Max(InSpacing, MinSpacing);

	Nodes.SetNum(Capacity);
	DirtyMask.Init(0, Capacity);

	Clear();
}

void FPetalTrailCore::Clear()
{
	HeadSerial = -1;
	NumValidSlots = 0;
	PetalCount = 0;
	bHasHead = false;
	LastHeadPosition = FVector::ZeroVector;
	DistanceCarry = 0.0f;

	if (Nodes.Num() != Capacity)
	{
		Nodes.SetNum(Capacity);
	}
	else
	{
		for (FPetalTrailNode& Node : Nodes)
		{
			Node.Serial = -1;
		}
	}

	DirtySlots.Reset();
	DirtyMask.Init(0, Capacity);
}

void FPetalTrailCore::Reset(const FVector& StartPosition)
{
	Clear();

	bHasHead = true;
	LastHeadPosition = StartPosition;

	// Anchor the trail at the head so a petal collected before the first move still has a home.
	PushNode(StartPosition);
}

void FPetalTrailCore::SetSpacing(float InSpacing)
{
	Spacing = FMath::Max(InSpacing, MinSpacing);

	// The old carry was measured against the old spacing; restart it rather than misplace a node.
	DistanceCarry = 0.0f;
}

void FPetalTrailCore::PushHead(const FVector& NewPosition)
{
	if (Capacity <= 0)
	{
		return;
	}

	if (!bHasHead)
	{
		Reset(NewPosition);
		return;
	}

	const FVector Segment = NewPosition - LastHeadPosition;
	const double Travel = Segment.Size();
	if (Travel <= UE_DOUBLE_SMALL_NUMBER)
	{
		return;
	}

	const FVector Direction = Segment / Travel;

	// Walk the segment in spacing-sized steps: a fast frame still produces evenly spaced nodes
	// instead of one long gap, and a slow frame carries its leftover distance forward.
	FVector Anchor = LastHeadPosition;
	double Offset = DistanceCarry;
	double Left = Travel;

	int32 Pushed = 0;
	while (Offset + Left >= Spacing && Pushed < MaxNodesPerPush)
	{
		const double Step = FMath::Max(Spacing - Offset, static_cast<double>(MinSpacing));
		Anchor += Direction * Step;
		PushNode(Anchor);

		Left -= Step;
		Offset = 0.0;
		++Pushed;
	}

	if (Pushed >= MaxNodesPerPush)
	{
		// The head moved further than one frame is allowed to record (teleport, hitch): drop the
		// tail of the segment instead of rewriting the entire ring.
		DistanceCarry = 0.0f;
	}
	else
	{
		DistanceCarry = static_cast<float>(Offset + FMath::Max(Left, 0.0));
	}

	LastHeadPosition = NewPosition;
}

void FPetalTrailCore::SetPetalCount(int32 InCount)
{
	const int32 NewCount = FMath::Clamp(InCount, 0, Capacity);
	if (NewCount == PetalCount)
	{
		return;
	}

	const int64 OldFirst = HeadSerial - PetalCount + 1;
	const int64 NewFirst = HeadSerial - NewCount + 1;

	// Whatever the direction, the slots that changed are the ones between the two window starts:
	// growing pulls serials in below the old start, shrinking pushes them out. Both run backwards,
	// so normalising the range makes one expression cover gain and loss alike.
	const int64 FirstChanged = FMath::Min(OldFirst, NewFirst);
	const int64 LastChanged = FMath::Max(OldFirst, NewFirst) - 1;
	MarkDirtyRange(FirstChanged, LastChanged);

	PetalCount = NewCount;
}

int32 FPetalTrailCore::AddPetals(int32 Count)
{
	if (Count > 0)
	{
		SetPetalCount(PetalCount + Count);
	}

	return PetalCount;
}

int32 FPetalTrailCore::ReleasePetals(int32 Count, int32& OutReleasedCount)
{
	OutReleasedCount = 0;
	if (Count > 0)
	{
		const int32 Previous = PetalCount;
		SetPetalCount(PetalCount - Count);
		OutReleasedCount = Previous - PetalCount;
	}

	return PetalCount;
}

void FPetalTrailCore::ClearDirtySlots()
{
	for (const int32 Slot : DirtySlots)
	{
		if (DirtyMask.IsValidIndex(Slot))
		{
			DirtyMask[Slot] = 0;
		}
	}

	DirtySlots.Reset();
}

bool FPetalTrailCore::IsSlotVisible(int32 Slot) const
{
	return Nodes.IsValidIndex(Slot) && IsSerialVisible(Nodes[Slot].Serial);
}

bool FPetalTrailCore::GetSlotPosition(int32 Slot, FVector& OutPosition) const
{
	if (!Nodes.IsValidIndex(Slot) || Nodes[Slot].Serial < 0)
	{
		return false;
	}

	OutPosition = Nodes[Slot].Position;
	return true;
}

bool FPetalTrailCore::GetSlotTangent(int32 Slot, FVector& OutTangent) const
{
	if (!Nodes.IsValidIndex(Slot) || Nodes[Slot].Serial < 0)
	{
		return false;
	}

	const int64 Serial = Nodes[Slot].Serial;
	const FVector Position = Nodes[Slot].Position;

	FVector Previous;
	FVector Next;
	const bool bHasPrevious = GetSerialPosition(Serial - 1, Previous);
	const bool bHasNext = GetSerialPosition(Serial + 1, Next);

	if (bHasPrevious && bHasNext)
	{
		OutTangent = (Next - Previous).GetSafeNormal();
	}
	else if (bHasPrevious)
	{
		OutTangent = (Position - Previous).GetSafeNormal();
	}
	else if (bHasNext)
	{
		OutTangent = (Next - Position).GetSafeNormal();
	}
	else
	{
		OutTangent = FVector::ForwardVector;
	}

	if (OutTangent.IsNearlyZero())
	{
		OutTangent = FVector::ForwardVector;
	}

	return true;
}

int32 FPetalTrailCore::SerialToSlot(int64 Serial, int32 InCapacity)
{
	if (InCapacity <= 0 || Serial < 0)
	{
		return INDEX_NONE;
	}

	return static_cast<int32>(Serial % InCapacity);
}

bool FPetalTrailCore::IsSerialInRing(int64 Serial) const
{
	if (Capacity <= 0 || Serial < 0 || HeadSerial < 0)
	{
		return false;
	}

	return Serial <= HeadSerial && Serial > HeadSerial - Capacity;
}

bool FPetalTrailCore::IsSerialVisible(int64 Serial) const
{
	if (PetalCount <= 0 || Serial < 0)
	{
		return false;
	}

	return Serial <= HeadSerial && Serial > HeadSerial - PetalCount;
}

bool FPetalTrailCore::GetSerialPosition(int64 Serial, FVector& OutPosition) const
{
	if (!IsSerialInRing(Serial))
	{
		return false;
	}

	const int32 Slot = SerialToSlot(Serial, Capacity);
	if (!Nodes.IsValidIndex(Slot) || Nodes[Slot].Serial != Serial)
	{
		return false;
	}

	OutPosition = Nodes[Slot].Position;
	return true;
}

void FPetalTrailCore::MarkSlotDirty(int64 Serial)
{
	if (!IsSerialInRing(Serial))
	{
		return;
	}

	const int32 Slot = SerialToSlot(Serial, Capacity);
	if (!DirtyMask.IsValidIndex(Slot) || DirtyMask[Slot] != 0)
	{
		return;
	}

	DirtyMask[Slot] = 1;
	DirtySlots.Add(Slot);
}

void FPetalTrailCore::MarkDirtyRange(int64 FirstSerial, int64 LastSerial)
{
	if (LastSerial < FirstSerial || Capacity <= 0)
	{
		return;
	}

	const int64 RingFirst = FMath::Max<int64>(HeadSerial - Capacity + 1, 0);
	const int64 From = FMath::Max(FirstSerial, RingFirst);
	const int64 To = FMath::Min(LastSerial, HeadSerial);

	for (int64 Serial = From; Serial <= To; ++Serial)
	{
		MarkSlotDirty(Serial);
	}
}

void FPetalTrailCore::PushNode(const FVector& Position)
{
	if (Capacity <= 0)
	{
		return;
	}

	const int64 Serial = ++HeadSerial;
	const int32 Slot = SerialToSlot(Serial, Capacity);

	Nodes[Slot].Position = Position;
	Nodes[Slot].Serial = Serial;
	NumValidSlots = FMath::Min(NumValidSlots + 1, Capacity);

	MarkSlotDirty(Serial);

	// The petal that just fell off the tail has to be parked. While the chain is shorter than the
	// ring that node is still around; once the chain fills the ring, the slot we just overwrote is
	// itself the one leaving the window, so the new node's own write covers it.
	if (PetalCount < Capacity)
	{
		MarkSlotDirty(Serial - PetalCount);
	}
}
