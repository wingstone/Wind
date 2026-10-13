// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PetalPickupField.generated.h"

class UPetalPickupFieldComponent;

/**
 * A patch of petals waiting to be collected, so one can be dropped into a level and scaled up.
 *
 * The actor is only a holder: all of the work happens in UPetalPickupFieldComponent, which is a
 * component precisely so it can also be added to an actor that already exists.
 */
UCLASS(meta = (DisplayName = "Petal Pickup Field"))
class APetalPickupField : public AActor
{
	GENERATED_BODY()

public:

	APetalPickupField();

	UFUNCTION(BlueprintPure, Category = "Petal Field")
	UPetalPickupFieldComponent* GetFieldComponent() const { return Field; }

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Petal Field", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPetalPickupFieldComponent> Field;
};
