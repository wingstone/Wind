// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "PetalTrailCore.h"
#include "PetalTrailComponent.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/** The trail gained or lost petals. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPetalTrailCountChanged, int32, PetalCount);

/** Petals were spent through ReleasePetals. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPetalTrailReleased, int32, ReleasedCount);

/**
 * The chain of petals that trails behind the player, like the tail of a snake.
 *
 * Petals are not actors and have no component of their own: the whole chain is a single instanced
 * static mesh, and the only thing simulated each frame is the path the owner has travelled along
 * (see FPetalTrailCore). Collecting a petal therefore costs one instance write no matter how many
 * petals the chain already holds, which is what keeps a few thousand petals affordable.
 *
 * Attach this to any stationary actor, or just leave it where you dropped it: instances are stored
 * in world space (see the absolute transform in OnRegister), so the parent must not drag them
 * around.
 */
UCLASS(ClassGroup = (Petal), meta = (BlueprintSpawnableComponent, DisplayName = "Petal Trail"))
class UPetalTrailComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	UPetalTrailComponent();

	// USceneComponent interface
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Petal accounting ---

	/** Adds petals and returns the new count. Stops at MaxPetals, dropping the oldest petals. */
	UFUNCTION(BlueprintCallable, Category = "Petal Trail")
	int32 AddPetals(int32 Count);

	/** Spends petals (opening a flower, paying a cost) and returns the new count. */
	UFUNCTION(BlueprintCallable, Category = "Petal Trail")
	int32 ReleasePetals(int32 Count);

	UFUNCTION(BlueprintCallable, Category = "Petal Trail")
	void SetPetalCount(int32 Count);

	UFUNCTION(BlueprintPure, Category = "Petal Trail")
	int32 GetPetalCount() const;

	UFUNCTION(BlueprintPure, Category = "Petal Trail")
	int32 GetMaxPetals() const { return MaxPetals; }

	/** Drops the path and the whole chain. */
	UFUNCTION(BlueprintCallable, Category = "Petal Trail")
	void ClearTrail();

	// --- Events ---

	UPROPERTY(BlueprintAssignable, Category = "Petal Trail|Events")
	FPetalTrailCountChanged OnPetalCountChanged;

	UPROPERTY(BlueprintAssignable, Category = "Petal Trail|Events")
	FPetalTrailReleased OnPetalsReleased;

	// --- Tuning ---

	/** Hard cap on the chain. Once reached, collecting another petal drops the oldest one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Trail", meta = (ClampMin = "1"))
	int32 MaxPetals = 3000;

	/** Distance between two petals along the path, in cm. Also the trail's resolution. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Trail", meta = (ClampMin = "1", Units = "cm"))
	float PetalSpacing = 15.0f;

	/** Optional follow target; the owner actor's location is used when this is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Trail")
	TObjectPtr<USceneComponent> FollowComponent;

	// --- Visuals ---

	/** Petal mesh. Defaults to the engine plane, so the system needs no art to run. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Trail|Visual")
	TObjectPtr<UStaticMesh> PetalMesh;

	/** Petal material. A real petal material can read custom data 0 (phase) and 1 (variation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Trail|Visual")
	TObjectPtr<UMaterialInterface> PetalMaterial;

	/** Scale applied to the mesh, whose own size is the reference. The default plane is 1 m wide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Trail|Visual", meta = (ClampMin = "0"))
	float PetalScale = 0.15f;

	/** Random scale spread, as a fraction of PetalScale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Trail|Visual", meta = (ClampMin = "0", ClampMax = "1"))
	float PetalScaleJitter = 0.35f;

	/** How far a petal is tilted away from the trail direction, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Trail|Visual", meta = (ClampMin = "0", ClampMax = "90"))
	float PetalTiltDegrees = 25.0f;

	/** Per-instance distances beyond which petals are culled, in cm. 0 disables the end distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Trail|Visual", meta = (ClampMin = "0", Units = "cm"))
	float InstanceCullDistance = 20000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Trail|Visual")
	int32 RandomSeed = 20261013;

	// --- Debug ---

	/** Draws the recorded path, so trail spacing can be checked in game. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Trail|Debug")
	bool bDrawDebug = false;

protected:

	/** Instanced mesh holding the whole chain; one instance per petal. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Petal Trail", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInstancedStaticMeshComponent> PetalInstances;

private:

	/** Creates the instances and the per-slot variation once, on first use. */
	void EnsureInstances();

	/** Writes the instance transforms the core marked dirty, then marks the render state once. */
	void FlushDirtySlots();

	FVector ResolveHeadLocation() const;
	FTransform BuildPetalTransform(int32 Slot, const FVector& Position, const FVector& Tangent) const;

	FPetalTrailCore Core;

	TArray<float> SlotScale;
	TArray<float> SlotRoll;
	TArray<float> SlotTilt;

	bool bInstancesReady = false;
};
