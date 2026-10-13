// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/Vector.h"

/**
 * One petal waiting to be collected, as plain data owned by a field's pool.
 *
 * A pickup has no UObject and no component of its own: it is one entry of the field's fixed pool
 * and one instance of the field's instanced mesh. Idle bobbing is done by the material, so
 * Position only changes while the petal is actually being pulled in by a magnet.
 */
struct FPetalPickup
{
	/** Where the petal was authored or generated. A respawning petal returns here. */
	FVector HomePosition = FVector::ZeroVector;

	/** Rest position in world space, or the live position while the petal is being pulled in. */
	FVector Position = FVector::ZeroVector;

	/** World-space velocity, only integrated while bAttracted is true. */
	FVector Velocity = FVector::ZeroVector;

	/** Per-petal scale multiplier on top of the field's base petal scale. */
	float Scale = 1.0f;

	/** Random phase handed to the material so neighbours do not bob in sync. */
	float Phase = 0.0f;

	/** Random variation index (hue, mesh variant, ...) handed to the material. */
	float Variation = 0.0f;

	/** Magnet radius override; <= 0 falls back to the magnet's own radius. */
	float AttractRadiusOverride = 0.0f;

	/** How many trail petals this pickup contributes when absorbed. */
	int32 Value = 1;

	/** Optional group tag, so gameplay code can address authored petals by group. */
	FName Group;

	/** Seconds to wait before respawning after being absorbed; < 0 means never respawn. */
	float RespawnDelay = -1.0f;

	/** Seconds left on the respawn timer; only meaningful while the petal is dead. */
	float RespawnCountdown = -1.0f;

	/** True while the petal can still be collected. */
	bool bAlive = true;

	/** True while the petal is being pulled toward a magnet; only then is it integrated on the CPU. */
	bool bAttracted = false;
};

/**
 * Deterministic per-instance variation shared by trail petals and field petals.
 *
 * The same index and seed always produce the same look, so an instance keeps its identity while
 * its ring slot is recycled - which is what lets the trail renderer update one instance per
 * recorded path node instead of rewriting the whole chain.
 */
struct FPetalVariation
{
	/** Scale applied to instances that are pooled but not currently shown. */
	static constexpr float HiddenScale = 1.0e-4f;

	/** Deterministic value in [0, 1) for a given (index, seed, stream) triple. */
	static float Hash01(int32 Index, int32 Seed, int32 Stream);

	static float MakeScale(int32 Index, int32 Seed, float BaseScale, float Jitter);
	static float MakeRollDegrees(int32 Index, int32 Seed);
	static float MakeTiltDegrees(int32 Index, int32 Seed, float MaxTiltDegrees);
	static float MakePhase(int32 Index, int32 Seed);
	static float MakeVariationIndex(int32 Index, int32 Seed, int32 VariationCount);

	/** Transform used to park a pooled instance that is not currently shown. */
	static FTransform MakeHiddenTransform();
};
