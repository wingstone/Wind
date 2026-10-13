// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "PetalPickupPool.h"
#include "PetalPickupFieldComponent.generated.h"

class APetalPickup;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class UPetalMagnetComponent;

/** A petal was absorbed by a magnet. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPetalPickupAbsorbed, FVector, Location, int32, PetalValue);

/**
 * Owns every petal waiting to be collected in one area, and the single instanced mesh that draws
 * them.
 *
 * Petals enter the field two ways and end up identical either way:
 *
 *   - placed by hand, as APetalPickup actors that this component captures at BeginPlay and then
 *     takes out of the picture, and
 *   - generated from FieldExtents and Density, for a broad drift of petals that nobody wants to
 *     place one at a time.
 *
 * Both become plain entries in FPetalPickupPool, so a field costs one draw call and one component
 * tick no matter how many petals it holds - and it does not tick at all until something is actually
 * being pulled in.
 */
UCLASS(ClassGroup = (Petal), meta = (BlueprintSpawnableComponent, DisplayName = "Petal Pickup Field"))
class UPetalPickupFieldComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	UPetalPickupFieldComponent();

	// USceneComponent interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Authoring ------------------------------------------------------------

	/** Captures every placed petal in the level that belongs to this field. Returns how many. */
	UFUNCTION(BlueprintCallable, Category = "Petal Field")
	int32 CapturePlacedPickups();

	/** Captures one placed petal. Returns 1 when it was taken over, 0 when it was already claimed. */
	int32 CapturePickup(APetalPickup* PlacedPetal);

	/** True when this field is the closest one to the given actor. */
	UFUNCTION(BlueprintPure, Category = "Petal Field")
	bool IsNearestFieldTo(const AActor* Other) const;

	// --- Pool -----------------------------------------------------------------

	/** Adds one petal and returns its pool index, which is also its instance index. */
	int32 AddPickup(const FTransform& Transform, int32 Value, FName Group, float Scale, float Variation, float Phase, float AttractRadiusOverride, float RespawnDelay);

	/** Pre-warms the pool and its instances, so a scripted burst of petals grows nothing mid-frame. */
	UFUNCTION(BlueprintCallable, Category = "Petal Field")
	void EnsurePickupCapacity(int32 Count);

	UFUNCTION(BlueprintPure, Category = "Petal Field")
	int32 GetPickupCount() const { return Pool.Num(); }

	UFUNCTION(BlueprintPure, Category = "Petal Field")
	int32 GetAlivePickupCount() const { return Pool.CountAlive(); }

	UFUNCTION(BlueprintPure, Category = "Petal Field")
	int32 GetAttractedPickupCount() const { return Pool.CountAttracted(); }

	/** Home transforms of the placed petals in a group, for gameplay to read a designed path. */
	UFUNCTION(BlueprintCallable, Category = "Petal Field")
	void GetPickupTransformsByGroup(FName Group, TArray<FTransform>& OutTransforms) const { Pool.GetTransformsByGroup(Group, OutTransforms); }

	// --- Collection -----------------------------------------------------------

	/** Flags up to MaxCount petals near Center as attracted. Called by magnets. */
	int32 AttractPickupsInRange(const FVector& Center, float Radius, UPetalMagnetComponent* Magnet, int32 MaxCount);

	/** Every petal field in a world, so magnets and placed petals can find their field. */
	static void ForEachRegisteredField(const UWorld* World, TFunctionRef<void(UPetalPickupFieldComponent*)> Visitor);

	static UPetalPickupFieldComponent* FindNearestField(const UWorld* World, const FVector& Location);

	// --- Layout ---------------------------------------------------------------

	/** Half-extents of the procedural petal volume, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Field", meta = (Units = "cm"))
	FVector FieldExtents = FVector(2000.0, 2000.0, 1000.0);

	/** Procedural petals per cubic metre. The volume times this is clamped by MaxProceduralPickups. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Field", meta = (ClampMin = "0"))
	float Density = 0.02f;

	/** Ceiling on procedurally generated petals, so a big volume cannot explode the pool. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Field", meta = (ClampMin = "0"))
	int32 MaxProceduralPickups = 2000;

	/** Take over the APetalPickup actors placed in the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Field")
	bool bCapturePlacedPickups = true;

	/** Destroy captured actors instead of leaving them hidden. Costs one short frame at level start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Field")
	bool bDestroyCapturedPickups = false;

	/** Procedural petals come back after being collected, so the field does not run dry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Field")
	bool bRespawnProceduralPickups = true;

	/** Seconds before a procedural petal returns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Field", meta = (ClampMin = "0"))
	float DefaultRespawnDelay = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Field")
	int32 RandomSeed = 20261013;

	/** Grid bucket size for the magnet query, in cm. Should be around the magnet radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Field", meta = (ClampMin = "1", Units = "cm"))
	float GridCellSize = 1500.0f;

	// --- Petal flight ---------------------------------------------------------

	/** How hard an attracted petal is pulled toward the magnet, in cm/s^2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Field|Flight", meta = (ClampMin = "0"))
	float PullAcceleration = 12000.0f;

	/** Velocity damping while flying in; higher means the petal settles on the magnet sooner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Field|Flight", meta = (ClampMin = "0"))
	float AttractDrag = 1.5f;

	/** Speed ceiling for an attracted petal, in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Field|Flight", meta = (ClampMin = "0", Units = "cm/s"))
	float MaxAttractSpeed = 6000.0f;

	// --- Visuals --------------------------------------------------------------

	/** Picked up from the engine plane when left empty, so the system needs no art to run. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Field|Visual")
	TObjectPtr<UStaticMesh> PickupMesh;

	/** Material for the petals. Custom data 0 is the bob phase, 1 the variation index. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Field|Visual")
	TObjectPtr<UMaterialInterface> PickupMaterial;

	/** Scale of a generated petal relative to the mesh's own size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Field|Visual", meta = (ClampMin = "0"))
	float PickupScale = 0.15f;

	/** Random scale spread of generated petals, as a fraction of PickupScale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Field|Visual", meta = (ClampMin = "0", ClampMax = "1"))
	float PickupScaleJitter = 0.35f;

	/** How many distinct variation indices generated petals are spread across. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Field|Visual", meta = (ClampMin = "1"))
	int32 RandomVariationCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Field|Visual", meta = (ClampMin = "0", Units = "cm"))
	float InstanceCullDistance = 20000.0f;

	// --- Events ---------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Petal Field|Events")
	FPetalPickupAbsorbed OnPickupAbsorbed;

	// --- Debug ----------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Field|Debug")
	bool bDrawDebug = false;

protected:

	/** Instance pool drawing every petal in this field. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Petal Field", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInstancedStaticMeshComponent> PickupInstances;

private:

	/** One petal being pulled in, and the magnet responsible for it. */
	struct FAttraction
	{
		int32 PickupIndex = INDEX_NONE;
		TWeakObjectPtr<UPetalMagnetComponent> Magnet;
	};

	void SeedProceduralPickups();

	/** Makes sure the instanced mesh has room for at least RequiredCount petals. */
	void EnsureInstances(int32 RequiredCount);

	void WritePickupInstance(int32 Index);
	void ParkPickupInstance(int32 Index);

	/** Fields only tick while they have something to do. */
	void UpdateTickEnabled();

	FPetalPickupPool Pool;

	TArray<FAttraction> Attractions;
	TArray<TWeakObjectPtr<APetalPickup>> PendingDestroy;

	/** Reused scratch buffers, so a query never allocates. */
	TArray<int32> CandidateScratch;
	TArray<int32> RespawnScratch;

	int32 CapturedPlacedCount = 0;
};
