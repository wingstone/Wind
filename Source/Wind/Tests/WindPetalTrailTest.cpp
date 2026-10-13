// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "PetalPickupPool.h"
#include "PetalTrailCore.h"
#include "PetalTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Slot a texture-like ring buffer keeps a serial in. Mirrors FPetalTrailCore's own mapping. */
	int32 SlotOfSerial(int64 Serial, int32 Capacity)
	{
		return static_cast<int32>(Serial % Capacity);
	}

	FPetalPickup MakePickup(const FVector& Home, int32 Value = 1, FName Group = NAME_None, float RespawnDelay = -1.0f)
	{
		FPetalPickup Pickup;
		Pickup.HomePosition = Home;
		Pickup.Value = Value;
		Pickup.Group = Group;
		Pickup.RespawnDelay = RespawnDelay;
		return Pickup;
	}
}

// ---------------------------------------------------------------------------
// Trail backbone
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPetalTrailSpacingTest,
	"Wind.Petals.Trail.Spacing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPetalTrailSpacingTest::RunTest(const FString& Parameters)
{
	FPetalTrailCore Core;
	Core.Initialize(64, 15.0f);
	Core.Reset(FVector::ZeroVector);

	const float Spacing = Core.GetSpacing();
	TestTrue(TEXT("Spacing is honoured"), FMath::IsNearlyEqual(Spacing, 15.0f));

	for (int32 Step = 1; Step <= 40; ++Step)
	{
		Core.PushHead(FVector(Step * Spacing, 0.0f, 0.0f));
	}

	// Walking in spacing-sized steps must produce one node per step, evenly spaced: this is the
	// property the whole petal chain rests on.
	int32 Measured = 0;
	float WorstError = 0.0f;
	FVector Previous = FVector::ZeroVector;
	bool bHasPrevious = false;

	for (int64 Serial = 0; Serial <= Core.GetHeadSerial(); ++Serial)
	{
		FVector Position;
		if (!Core.GetSlotPosition(SlotOfSerial(Serial, Core.GetCapacity()), Position))
		{
			continue;
		}

		if (bHasPrevious)
		{
			WorstError = FMath::Max(WorstError, FMath::Abs(static_cast<float>(FVector::Dist(Position, Previous)) - Spacing));
			++Measured;
		}

		Previous = Position;
		bHasPrevious = true;
	}

	TestTrue(TEXT("Every step recorded a node"), Measured >= 39);
	TestTrue(TEXT("Nodes stay evenly spaced"), WorstError < 0.1f);

	// A half-step must not produce a node, but two half-steps must.
	Core.ClearDirtySlots();
	Core.PushHead(FVector(40 * Spacing + Spacing * 0.5f, 0.0f, 0.0f));
	TestEqual(TEXT("Half a spacing does not record a node"), Core.GetDirtySlots().Num(), 0);

	Core.PushHead(FVector(40 * Spacing + Spacing, 0.0f, 0.0f));
	TestEqual(TEXT("The carried half-step does record a node"), Core.GetDirtySlots().Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPetalTrailRingWrapTest,
	"Wind.Petals.Trail.RingWrap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPetalTrailRingWrapTest::RunTest(const FString& Parameters)
{
	const int32 Capacity = 8;
	FPetalTrailCore Core;
	Core.Initialize(Capacity, 10.0f);
	Core.Reset(FVector::ZeroVector);

	for (int32 Step = 1; Step <= Capacity * 3; ++Step)
	{
		Core.PushHead(FVector(Step * 10.0f, 0.0f, 0.0f));
	}

	const int64 Head = Core.GetHeadSerial();
	TestEqual(TEXT("Serials keep counting past the ring"), Head, static_cast<int64>(Capacity * 3));
	TestEqual(TEXT("Live slots saturate at the capacity"), Core.GetNumValidSlots(), Capacity);

	// The oldest serial still in the ring is the one node before the capacity ran out.
	FVector Oldest;
	TestTrue(TEXT("Oldest surviving node is readable"),
		Core.GetSlotPosition(SlotOfSerial(Head - Capacity + 1, Capacity), Oldest));
	TestTrue(TEXT("Oldest surviving node holds its own position"),
		FMath::IsNearlyEqual(static_cast<float>(Oldest.X), static_cast<float>(Head - Capacity + 1) * 10.0f, 0.1f));

	FVector Head2;
	TestTrue(TEXT("Newest node is readable"), Core.GetSlotPosition(SlotOfSerial(Head, Capacity), Head2));
	TestTrue(TEXT("Newest node holds the newest position"),
		FMath::IsNearlyEqual(static_cast<float>(Head2.X), static_cast<float>(Head) * 10.0f, 0.1f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPetalTrailPetalWindowTest,
	"Wind.Petals.Trail.PetalWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPetalTrailPetalWindowTest::RunTest(const FString& Parameters)
{
	const int32 Capacity = 32;
	FPetalTrailCore Core;
	Core.Initialize(Capacity, 10.0f);
	Core.Reset(FVector::ZeroVector);

	for (int32 Step = 1; Step <= 20; ++Step)
	{
		Core.PushHead(FVector(Step * 10.0f, 0.0f, 0.0f));
	}

	const int64 Head = Core.GetHeadSerial();
	TestEqual(TEXT("Twenty pushes plus the anchor"), Head, static_cast<int64>(20));

	TestEqual(TEXT("A fresh trail shows nothing"), Core.GetPetalCount(), 0);
	TestFalse(TEXT("Nothing is visible at zero petals"), Core.IsSlotVisible(SlotOfSerial(Head, Capacity)));

	Core.SetPetalCount(5);
	TestEqual(TEXT("Petal count is applied"), Core.GetPetalCount(), 5);
	TestTrue(TEXT("Newest petal of the window is visible"), Core.IsSlotVisible(SlotOfSerial(Head, Capacity)));
	TestTrue(TEXT("Oldest petal of the window is visible"), Core.IsSlotVisible(SlotOfSerial(Head - 4, Capacity)));
	TestFalse(TEXT("The petal just outside the window is parked"), Core.IsSlotVisible(SlotOfSerial(Head - 5, Capacity)));

	int32 Released = 0;
	Core.ReleasePetals(2, Released);
	TestEqual(TEXT("Releasing reports what it actually spent"), Released, 2);
	TestEqual(TEXT("Releasing shrinks the chain"), Core.GetPetalCount(), 3);
	TestFalse(TEXT("The released petal is parked"), Core.IsSlotVisible(SlotOfSerial(Head - 3, Capacity)));
	TestTrue(TEXT("The released petal is still the oldest visible one"), Core.IsSlotVisible(SlotOfSerial(Head - 2, Capacity)));

	Core.AddPetals(1000);
	TestEqual(TEXT("The chain never exceeds the cap"), Core.GetPetalCount(), Capacity);
	TestEqual(TEXT("Asking for petals on an empty trail is harmless"), Core.AddPetals(-5), Capacity);

	Core.ReleasePetals(1000, Released);
	TestEqual(TEXT("All petals can be spent"), Core.GetPetalCount(), 0);
	TestFalse(TEXT("Nothing is visible once the chain is emptied"), Core.IsSlotVisible(SlotOfSerial(Head, Capacity)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPetalTrailCapDropsOldestTest,
	"Wind.Petals.Trail.CapDropsOldest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPetalTrailCapDropsOldestTest::RunTest(const FString& Parameters)
{
	const int32 Capacity = 16;
	FPetalTrailCore Core;
	Core.Initialize(Capacity, 5.0f);
	Core.Reset(FVector::ZeroVector);

	for (int32 Step = 1; Step <= 40; ++Step)
	{
		Core.PushHead(FVector(Step * 5.0f, 0.0f, 0.0f));
	}

	Core.SetPetalCount(999);
	TestEqual(TEXT("A full chain sits at the cap"), Core.GetPetalCount(), Capacity);

	// The chain is identified by serials, not by slots: once the ring wraps, the slot the dropped
	// petal used is the very slot the new head lands in.
	FVector Position;
	int32 VisibleCount = 0;
	bool bHasOldest = false;
	float OldestX = 0.0f;

	for (int32 Slot = 0; Slot < Capacity; ++Slot)
	{
		if (Core.IsSlotVisible(Slot) && Core.GetSlotPosition(Slot, Position))
		{
			++VisibleCount;
			if (!bHasOldest || static_cast<float>(Position.X) < OldestX)
			{
				OldestX = static_cast<float>(Position.X);
				bHasOldest = true;
			}
		}
	}

	TestEqual(TEXT("A full chain shows exactly the cap"), VisibleCount, Capacity);

	const int64 Head = Core.GetHeadSerial();
	TestTrue(TEXT("The tail of a full chain is the node one capacity behind the head"),
		FMath::IsNearlyEqual(OldestX, static_cast<float>(Head - Capacity + 1) * 5.0f, 0.1f));

	// One more node and the whole window slides: the tail drops off on its own, so being at the cap
	// costs exactly the same as being below it.
	Core.PushHead(FVector(41 * 5.0f, 0.0f, 0.0f));

	const int64 NewHead = Core.GetHeadSerial();
	TestEqual(TEXT("The head moved on"), NewHead, Head + 1);
	TestEqual(TEXT("The chain did not grow past the cap"), Core.GetPetalCount(), Capacity);

	VisibleCount = 0;
	bHasOldest = false;

	for (int32 Slot = 0; Slot < Capacity; ++Slot)
	{
		if (Core.IsSlotVisible(Slot) && Core.GetSlotPosition(Slot, Position))
		{
			++VisibleCount;
			if (!bHasOldest || static_cast<float>(Position.X) < OldestX)
			{
				OldestX = static_cast<float>(Position.X);
				bHasOldest = true;
			}
		}
	}

	TestEqual(TEXT("The dropped petal was replaced one for one"), VisibleCount, Capacity);
	TestTrue(TEXT("The oldest petal was dropped and the window slid forward"),
		FMath::IsNearlyEqual(OldestX, static_cast<float>(NewHead - Capacity + 1) * 5.0f, 0.1f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPetalTrailDirtyBudgetTest,
	"Wind.Petals.Trail.DirtyBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPetalTrailDirtyBudgetTest::RunTest(const FString& Parameters)
{
	// The whole point of the ring: a full-size chain must not cost more per frame than an empty one.
	const int32 Capacity = 3000;
	FPetalTrailCore Core;
	Core.Initialize(Capacity, 15.0f);
	Core.Reset(FVector::ZeroVector);
	Core.SetPetalCount(Capacity);
	Core.ClearDirtySlots();

	// 500 cm/s at 60 fps for three frames.
	Core.PushHead(FVector(25.0f, 0.0f, 0.0f));
	TestTrue(TEXT("A 3000 petal chain dirties a handful of instances per frame"), Core.GetDirtySlots().Num() <= 4);

	Core.ClearDirtySlots();
	Core.PushHead(FVector(100000.0f, 0.0f, 0.0f));
	TestTrue(TEXT("Even a long frame stays inside the node budget"),
		Core.GetDirtySlots().Num() <= Core.MaxNodesPerPush + 1);

	// Growing and shrinking only touch the slots the window actually gained or lost. The chain has
	// to be long enough for those slots to exist, which is the normal case in play.
	const int32 WindowCapacity = 32;
	FPetalTrailCore Window;
	Window.Initialize(WindowCapacity, 10.0f);
	Window.Reset(FVector::ZeroVector);

	for (int32 Step = 1; Step <= WindowCapacity * 2; ++Step)
	{
		Window.PushHead(FVector(Step * 10.0f, 0.0f, 0.0f));
	}

	Window.SetPetalCount(WindowCapacity);
	Window.ClearDirtySlots();
	Window.SetPetalCount(WindowCapacity - 1);
	TestEqual(TEXT("Losing a petal dirties exactly one slot"), Window.GetDirtySlots().Num(), 1);

	Window.ClearDirtySlots();
	Window.SetPetalCount(WindowCapacity);
	TestEqual(TEXT("Gaining a petal dirties exactly one slot"), Window.GetDirtySlots().Num(), 1);

	int32 Spent = 0;
	Window.ClearDirtySlots();
	Window.ReleasePetals(10, Spent);
	TestEqual(TEXT("Releasing ten petals dirties ten slots"), Window.GetDirtySlots().Num(), 10);

	Window.ClearDirtySlots();
	Window.SetPetalCount(WindowCapacity);
	TestEqual(TEXT("Refilling the chain dirties ten slots"), Window.GetDirtySlots().Num(), 10);

	// A short chain has nothing to park: the serials that would join or leave do not exist yet.
	FPetalTrailCore Fresh;
	Fresh.Initialize(Capacity, 15.0f);
	Fresh.Reset(FVector::ZeroVector);
	Fresh.SetPetalCount(Capacity - 1);
	Fresh.ClearDirtySlots();
	Fresh.SetPetalCount(Capacity);
	TestEqual(TEXT("Claiming petals the trail has not walked yet dirties nothing"), Fresh.GetDirtySlots().Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPetalTrailGuardTest,
	"Wind.Petals.Trail.Guard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPetalTrailGuardTest::RunTest(const FString& Parameters)
{
	// A zero spacing would divide the movement into zero-length steps, so it is floored instead of
	// producing an infinite loop or a NaN.
	FPetalTrailCore Core;
	Core.Initialize(8, 0.0f);
	TestTrue(TEXT("Spacing is floored above zero"), Core.GetSpacing() >= FPetalTrailCore::MinSpacing);

	Core.SetSpacing(-100.0f);
	TestTrue(TEXT("A negative spacing is floored too"), Core.GetSpacing() >= FPetalTrailCore::MinSpacing);

	Core.Reset(FVector::ZeroVector);
	for (int32 Step = 1; Step <= 200; ++Step)
	{
		Core.PushHead(FVector(Step, 0.0f, 0.0f));
	}

	FVector Position;
	TestTrue(TEXT("The trail still has a head"), Core.GetSlotPosition(SlotOfSerial(Core.GetHeadSerial(), Core.GetCapacity()), Position));
	TestFalse(TEXT("Walking a zero spacing produced no NaN"), Position.ContainsNaN());

	// A teleport must not turn into a frame that rewrites the whole ring.
	FPetalTrailCore Teleporting;
	Teleporting.Initialize(4096, 10.0f);
	Teleporting.Reset(FVector::ZeroVector);
	Teleporting.ClearDirtySlots();
	Teleporting.PushHead(FVector(1000000.0f, 0.0f, 0.0f));

	TestTrue(TEXT("A teleport records at most MaxNodesPerPush nodes"),
		Teleporting.GetDirtySlots().Num() <= Teleporting.MaxNodesPerPush + 1);

	// Zero capacity must not crash or record anything.
	FPetalTrailCore Empty;
	Empty.Initialize(0, 10.0f);
	Empty.Reset(FVector::ZeroVector);
	Empty.PushHead(FVector(1000.0f, 0.0f, 0.0f));
	TestEqual(TEXT("A zero capacity trail holds nothing"), Empty.GetPetalCount(), 0);
	TestEqual(TEXT("A zero capacity trail records nothing"), Empty.GetNumValidSlots(), 0);

	return true;
}

// ---------------------------------------------------------------------------
// Pickup pool
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPetalPoolAddAndRecycleTest,
	"Wind.Petals.Pool.AddAndRecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPetalPoolAddAndRecycleTest::RunTest(const FString& Parameters)
{
	FPetalPickupPool Pool;
	Pool.SetCellSize(1000.0f);

	const int32 Index = Pool.AddPickup(MakePickup(FVector(100.0f, 0.0f, 0.0f), 3, TEXT("Path"), 2.0f));

	TestEqual(TEXT("The pool holds one petal"), Pool.Num(), 1);
	TestTrue(TEXT("The petal is valid"), Pool.IsValidIndex(Index));
	TestTrue(TEXT("The petal is alive"), Pool.IsAlive(Index));
	TestEqual(TEXT("The petal keeps its value"), Pool.GetPickup(Index).Value, 3);
	TestTrue(TEXT("A pooled petal starts at its home position"), Pool.GetPickup(Index).Position.Equals(FVector(100.0f, 0.0f, 0.0f)));

	Pool.SetAttracted(Index, true);
	TestEqual(TEXT("The petal is flagged attracted"), Pool.CountAttracted(), 1);

	TArray<int32> Found;
	Pool.GatherNearby(FVector(100.0f, 0.0f, 0.0f), 500.0f, Found);
	TestEqual(TEXT("An attracted petal leaves the grid"), Found.Num(), 0);

	Pool.Absorb(Index, 2.0f);
	TestFalse(TEXT("The petal is collected"), Pool.IsAlive(Index));
	TestEqual(TEXT("It is not attracted any more"), Pool.CountAttracted(), 0);
	TestEqual(TEXT("It is waiting on its respawn timer"), Pool.CountPendingRespawn(), 1);

	TArray<int32> Respawned;
	TestEqual(TEXT("Nothing respawns early"), Pool.TickRespawn(1.0f, Respawned), 0);
	TestEqual(TEXT("The petal comes back on time"), Pool.TickRespawn(1.5f, Respawned), 1);
	TestTrue(TEXT("The petal is alive again"), Pool.IsAlive(Index));
	TestTrue(TEXT("A respawned petal returns home"), Pool.GetPickup(Index).Position.Equals(FVector(100.0f, 0.0f, 0.0f)));
	TestEqual(TEXT("The respawn queue is empty"), Pool.CountPendingRespawn(), 0);
	TestEqual(TEXT("The index is reused in place"), Index, 0);

	Pool.GatherNearby(FVector(100.0f, 0.0f, 0.0f), 500.0f, Found);
	TestEqual(TEXT("A respawned petal is back in the grid"), Found.Num(), 1);

	// A petal that never respawns stays collected, and never resurrects.
	Pool.Absorb(Index, -1.0f);
	Respawned.Reset();
	TestEqual(TEXT("A one-shot petal never returns"), Pool.TickRespawn(1000.0f, Respawned), 0);
	TestFalse(TEXT("A one-shot petal stays collected"), Pool.IsAlive(Index));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPetalPoolGrowthTest,
	"Wind.Petals.Pool.GrowthKeepsIndices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPetalPoolGrowthTest::RunTest(const FString& Parameters)
{
	FPetalPickupPool Pool;
	Pool.SetCellSize(1000.0f);

	TArray<int32> EarlyIndices;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		EarlyIndices.Add(Pool.AddPickup(MakePickup(FVector(Index * 500.0f, 0.0f, 0.0f))));
	}

	// Push the pool through several growth chunks.
	for (int32 Index = 3; Index < 600; ++Index)
	{
		Pool.AddPickup(MakePickup(FVector(Index * 500.0f, 0.0f, 0.0f)));
	}

	TestEqual(TEXT("Every petal was kept"), Pool.Num(), 600);

	for (int32 Index = 0; Index < 3; ++Index)
	{
		TestTrue(TEXT("An early index is still valid after growth"), Pool.IsValidIndex(EarlyIndices[Index]));
		TestTrue(TEXT("An early index still holds its own petal"),
			Pool.GetPickup(EarlyIndices[Index]).HomePosition.Equals(FVector(Index * 500.0f, 0.0f, 0.0f)));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPetalPoolGridTest,
	"Wind.Petals.Pool.GridQuery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPetalPoolGridTest::RunTest(const FString& Parameters)
{
	FPetalPickupPool Pool;
	Pool.SetCellSize(500.0f);

	const int32 NearIndex = Pool.AddPickup(MakePickup(FVector(100.0f, 0.0f, 0.0f)));
	const int32 OriginIndex = Pool.AddPickup(MakePickup(FVector(0.0f, 0.0f, 0.0f)));
	Pool.AddPickup(MakePickup(FVector(50000.0f, 0.0f, 0.0f)));

	TArray<int32> Found;
	Pool.GatherNearby(FVector::ZeroVector, 600.0f, Found);

	TestEqual(TEXT("Only petals inside the radius are returned"), Found.Num(), 2);
	TestTrue(TEXT("The petal inside the radius is returned"), Found.Contains(NearIndex));
	TestTrue(TEXT("The petal on top of the center is returned"), Found.Contains(OriginIndex));

	Found.Reset();
	Pool.GatherNearby(FVector::ZeroVector, 0.0f, Found);
	TestEqual(TEXT("A zero radius finds nothing"), Found.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPetalPoolGroupTest,
	"Wind.Petals.Pool.GroupQuery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPetalPoolGroupTest::RunTest(const FString& Parameters)
{
	FPetalPickupPool Pool;
	Pool.SetCellSize(1000.0f);

	Pool.AddPickup(MakePickup(FVector(0.0f, 0.0f, 0.0f), 1, TEXT("Path")));
	Pool.AddPickup(MakePickup(FVector(100.0f, 0.0f, 0.0f), 1, TEXT("Path")));
	Pool.AddPickup(MakePickup(FVector(0.0f, 200.0f, 0.0f), 1, TEXT("Ring")));
	Pool.AddPickup(MakePickup(FVector(0.0f, 300.0f, 0.0f)));

	TArray<FTransform> Transforms;
	Pool.GetTransformsByGroup(TEXT("Path"), Transforms);
	TestEqual(TEXT("Only the requested group is returned"), Transforms.Num(), 2);

	Transforms.Reset();
	Pool.GetTransformsByGroup(TEXT("Nowhere"), Transforms);
	TestEqual(TEXT("An unknown group returns nothing"), Transforms.Num(), 0);

	Transforms.Reset();
	Pool.GetTransformsByGroup(NAME_None, Transforms);
	TestEqual(TEXT("Ungrouped petals are addressable as NAME_None"), Transforms.Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPetalPoolAttractMathTest,
	"Wind.Petals.Pool.AttractMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPetalPoolAttractMathTest::RunTest(const FString& Parameters)
{
	FPetalPickupPool Pool;
	Pool.SetCellSize(1000.0f);

	const int32 Index = Pool.AddPickup(MakePickup(FVector(1000.0f, 0.0f, 0.0f)));
	Pool.SetAttracted(Index, true);

	const FVector Target = FVector::ZeroVector;
	const float DeltaTime = 1.0f / 60.0f;

	const float StartDistance = static_cast<float>(FVector::Dist(Pool.GetPickup(Index).Position, Target));
	Pool.StepAttracted(Index, Target, DeltaTime, 12000.0f, 1.5f, 6000.0f);
	const float AfterOneStep = static_cast<float>(FVector::Dist(Pool.GetPickup(Index).Position, Target));

	TestTrue(TEXT("An attracted petal moves toward the magnet"), AfterOneStep < StartDistance);

	for (int32 Step = 0; Step < 240; ++Step)
	{
		Pool.StepAttracted(Index, Target, DeltaTime, 12000.0f, 1.5f, 6000.0f);
	}

	TestFalse(TEXT("Flying produced no NaN"), Pool.GetPickup(Index).Position.ContainsNaN());
	TestTrue(TEXT("A petal makes it to the mask's absorb radius"), Pool.HasReachedTarget(Index, Target, 120.0f));

	// Guards: a magnet with no pull, no damping or no radius must still be well behaved.
	Pool.StepAttracted(Index, Target, 0.0f, 0.0f, 0.0f, 0.0f);
	Pool.StepAttracted(Index, Target, DeltaTime, 0.0f, 0.0f, 0.0f);
	TestFalse(TEXT("Zero pull produces no NaN"), Pool.GetPickup(Index).Position.ContainsNaN());
	TestTrue(TEXT("A zero absorb radius only counts an exact hit"),
		Pool.HasReachedTarget(Index, Pool.GetPickup(Index).Position, 0.0f));

	// Only petals a magnet flagged are integrated, so nothing else in the field ever pays for this.
	Pool.SetAttracted(Index, false);
	TestFalse(TEXT("A petal that is not attracted is not integrated"),
		Pool.StepAttracted(Index, Target, DeltaTime, 1000.0f, 0.0f, 1000.0f));

	return true;
}

// ---------------------------------------------------------------------------
// Per-instance variation
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPetalVariationTest,
	"Wind.Petals.Variation.Deterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPetalVariationTest::RunTest(const FString& Parameters)
{
	// A slot keeps its look while the ring recycles it, so the values must never drift.
	TestEqual(TEXT("Scale is stable for an index"),
		FPetalVariation::MakeScale(7, 1234, 0.15f, 0.35f), FPetalVariation::MakeScale(7, 1234, 0.15f, 0.35f), 0.0f);
	TestEqual(TEXT("Roll is stable for an index"),
		FPetalVariation::MakeRollDegrees(7, 1234), FPetalVariation::MakeRollDegrees(7, 1234), 0.0f);
	TestEqual(TEXT("Phase is stable for an index"),
		FPetalVariation::MakePhase(7, 1234), FPetalVariation::MakePhase(7, 1234), 0.0f);

	TestTrue(TEXT("Neighbouring petals do not share a roll"),
		!FMath::IsNearlyEqual(FPetalVariation::MakeRollDegrees(1, 7), FPetalVariation::MakeRollDegrees(2, 7)));

	const float BaseScale = 1.0f;
	const float Jitter = 0.35f;
	for (int32 Index = 0; Index < 256; ++Index)
	{
		const float Scale = FPetalVariation::MakeScale(Index, 99, BaseScale, Jitter);
		TestTrue(TEXT("Scale stays inside the requested jitter"),
			Scale >= BaseScale * (1.0f - Jitter) - KINDA_SMALL_NUMBER && Scale <= BaseScale * (1.0f + Jitter) + KINDA_SMALL_NUMBER);

		const float Roll = FPetalVariation::MakeRollDegrees(Index, 99);
		TestTrue(TEXT("Roll stays on the circle"), Roll >= 0.0f && Roll < 360.0f);

		TestTrue(TEXT("Variation index respects the count"), FPetalVariation::MakeVariationIndex(Index, 99, 4) < 4.0f);
	}

	// Parked instances keep a real, tiny scale: zero would be degenerate, visible would be wrong.
	const FTransform Hidden = FPetalVariation::MakeHiddenTransform();
	TestTrue(TEXT("A parked petal is scaled down, not collapsed"),
		Hidden.GetScale3D().GetMax() > 0.0f && Hidden.GetScale3D().GetMax() < 0.01f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
