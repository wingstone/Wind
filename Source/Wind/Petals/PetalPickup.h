// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PetalPickup.generated.h"

class UBillboardComponent;
class UPetalPickupCollectableComponent;
class UStaticMeshComponent;

/**
 * A single petal the designer drops into the level by hand.
 *
 * The actor exists so petals can be placed and tuned per position - a guide path, a ring, a trail
 * of breadcrumbs - but it is an authoring proxy, not the runtime representation: at BeginPlay the
 * nearest petal field converts it into a pooled petal (plain data plus one instanced mesh slot)
 * and the actor steps aside with its tick, collision and visibility disabled.
 *
 * Keep an actor alive as a real petal by turning on bAuthoringOnly: that path is for hero petals
 * that need their own mesh, material or blueprint logic, and it is meant for a handful of pieces
 * rather than hundreds.
 */
UCLASS(meta = (DisplayName = "Petal Pickup"))
class APetalPickup : public AActor
{
	GENERATED_BODY()

public:

	APetalPickup();

	// AActor interface
	virtual void BeginPlay() override;

	/** Hands this petal to the field closest to it, if it has not been captured yet. */
	UFUNCTION(BlueprintCallable, Category = "Petal Pickup")
	void RegisterToNearestField();

	/** True once a field has taken over this petal and the actor no longer represents it. */
	UFUNCTION(BlueprintPure, Category = "Petal Pickup")
	bool IsCaptured() const { return bCaptured; }

	/** Called by the field that captured this petal: the actor gets out of the way. */
	void OnCaptured();

	bool IsAuthoringOnly() const { return bAuthoringOnly; }
	bool ShouldCaptureOnBeginPlay() const { return bCaptureOnBeginPlay && !bAuthoringOnly; }

	int32 GetPetalValue() const { return FMath::Max(PetalValue, 0); }
	FName GetGroup() const { return Group; }
	float GetPetalScale() const { return FMath::Max(Scale, 0.0f); }
	float GetVariation() const { return Variation; }
	float GetAttractRadiusOverride() const { return FMath::Max(AttractRadiusOverride, 0.0f); }
	float GetPickupRespawnDelay() const { return bRespawn ? FMath::Max(RespawnDelay, 0.0f) : -1.0f; }

	/** Deterministic phase derived from the placement, so hand-placed petals do not bob in sync. */
	float GetPickupPhase() const;

protected:

	/** Scene root. Also the transform the field samples when it captures this petal. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Petal Pickup", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	/** Editor handle so the petal is easy to see, select and drag in the viewport. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Petal Pickup", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBillboardComponent> Handle;

	/** Only used by bAuthoringOnly petals, which keep their own visual. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Petal Pickup", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> HeroMesh;

	/** Only active for bAuthoringOnly petals, which do their own distance and absorb test. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Petal Pickup", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPetalPickupCollectableComponent> Collectable;

	// --- Gameplay parameters carried into the field's pool ---

	/** How many trail petals this petal is worth. Use > 1 for a fat, obviously valuable petal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Pickup", meta = (ClampMin = "0"))
	int32 PetalValue = 1;

	/** Random look index handed to the petal material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Pickup")
	float Variation = 0.0f;

	/** Scale relative to the petal mesh's own size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Pickup", meta = (ClampMin = "0"))
	float Scale = 1.0f;

	/** Regrow this petal after it is collected, so a path can be flown more than once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Pickup")
	bool bRespawn = false;

	/** Seconds before the petal comes back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Pickup", meta = (ClampMin = "0", EditCondition = "bRespawn"))
	float RespawnDelay = 45.0f;

	/** Overrides the magnet radius locally; 0 uses the magnet's own radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Pickup", meta = (ClampMin = "0"))
	float AttractRadiusOverride = 0.0f;

	/** Optional tag, so gameplay code can ask a field for this group's positions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Pickup")
	FName Group;

	/** False leaves the petal entirely under script control. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Pickup")
	bool bCaptureOnBeginPlay = true;

	/** Keep this petal as a real actor: it renders and collects itself instead of joining a pool. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Pickup")
	bool bAuthoringOnly = false;

private:

	bool bCaptured = false;
};
