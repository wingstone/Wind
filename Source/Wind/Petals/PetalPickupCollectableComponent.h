// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "PetalPickupCollectableComponent.generated.h"

class UPetalMagnetComponent;

/**
 * Lets an actor collect itself.
 *
 * Only "hero" petals need this: the ones that keep their own mesh and blueprint logic instead of
 * being swallowed into a petal field's pool. Everything else should go through
 * UPetalPickupFieldComponent, which is pooled and instanced precisely so that hundreds of petals do
 * not each pay for a component tick.
 */
UCLASS(ClassGroup = (Petal), meta = (BlueprintSpawnableComponent, DisplayName = "Petal Collectable"))
class UPetalPickupCollectableComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	UPetalPickupCollectableComponent();

	// USceneComponent interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Turns collection on or off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Collectable")
	bool bEnable = true;

	/** How many trail petals this petal is worth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Collectable", meta = (ClampMin = "0"))
	int32 PetalValue = 1;

	/** Glide speed toward the magnet once inside its radius, in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Collectable", meta = (ClampMin = "0", Units = "cm/s"))
	float ApproachSpeed = 1200.0f;

	/** Destroy the owner actor once collected. Turn off to keep it around for scripted reuse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Collectable")
	bool bDestroyAfterCollect = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal Collectable|Debug")
	bool bDrawDebug = false;

private:

	void Collect(UPetalMagnetComponent& Magnet);
};
