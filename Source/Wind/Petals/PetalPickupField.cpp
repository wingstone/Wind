// Copyright Epic Games, Inc. All Rights Reserved.

#include "PetalPickupField.h"
#include "Components/SceneComponent.h"
#include "PetalPickupFieldComponent.h"

APetalPickupField::APetalPickupField()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Field = CreateDefaultSubobject<UPetalPickupFieldComponent>(TEXT("Field"));
	Field->SetupAttachment(SceneRoot);
}
