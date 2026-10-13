// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "PetalMagnetComponent.generated.h"

class UPetalPickupFieldComponent;
class UPetalTrailComponent;

/** A petal reached the magnet and joined the trail. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPetalCollected, FVector, Location, int32, PetalValue);

/**
 * Collects petals for whatever it is attached to.
 *
 * The magnet does no work per petal: it asks each registered petal field for the petals near it,
 * caps how many it flags per frame, and then lets the field do the flying and the absorbing. Fields
 * only tick while they actually have something to pull in, so a magnet sitting still in an empty
 * part of the level costs a single nearest-grid-neighbour lookup per field.
 */
UCLASS(ClassGroup = (Petal), meta = (BlueprintSpawnableComponent, DisplayName = "Petal Magnet"))
class UPetalMagnetComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	UPetalMagnetComponent();

	// USceneComponent interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** World position petals are pulled toward. */
	UFUNCTION(BlueprintPure, Category = "Petal Magnet")
	FVector GetMagnetLocation() const;

	/** Petals inside this radius start flying in, in cm. */
	UFUNCTION(BlueprintPure, Category = "Petal Magnet")
	float GetMagnetRadius() const { return MagnetRadius; }

	/** A petal this close is counted as collected, in cm. */
	UFUNCTION(BlueprintPure, Category = "Petal Magnet")
	float GetAbsorbRadius() const { return AbsorbRadius; }

	/** Called by the field responsible for a flagged petal once it arrives. */
	void NotifyPickupAbsorbed(const FVector& Location, int32 PetalValue);

	/** Every magnet in a world, so placed hero petals can find their collector. */
	static void ForEachRegisteredMagnet(const UWorld* World, TFunctionRef<void(UPetalMagnetComponent*)> Visitor);

	/** Nearest registered magnet to a location; OutDistanceSquared is only valid on a hit. */
	static UPetalMagnetComponent* FindNearestMagnet(const UWorld* World, const FVector& Location, float& OutDistanceSquared);

	// --- Configuration ---

	/** Turns collection off without detaching anything. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Magnet")
	bool bEnableMagnet = true;

	/** Petals that come closer than this are pulled in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Magnet", meta = (ClampMin = "0", Units = "cm"))
	float MagnetRadius = 1500.0f;

	/** A petal this close to the magnet counts as collected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Magnet", meta = (ClampMin = "0", Units = "cm"))
	float AbsorbRadius = 120.0f;

	/** Caps how many petals can be pulled in at once, so flying through a field cannot spike. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Magnet", meta = (ClampMin = "0"))
	int32 MaxAttractPerFrame = 64;

	/** Trail that receives the petals. Resolved from the owner actor when left empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Magnet")
	TObjectPtr<UPetalTrailComponent> Trail;

	/** Optional collection origin; the owner actor's location is used when this is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Magnet")
	TObjectPtr<USceneComponent> SourceComponent;

	/** A petal was collected. */
	UPROPERTY(BlueprintAssignable, Category = "Petal Magnet|Events")
	FPetalCollected OnPetalCollected;

	// --- Debug ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Magnet|Debug")
	bool bDrawDebug = false;
};
