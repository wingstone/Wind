// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WindFlierGameMode.generated.h"

/**
 * Game mode for the standalone wind flight 3C.
 *
 * Unlike the other variants' game modes, which are abstract bases for Blueprint
 * subclasses, this one is concrete and wires itself up: the flight controller
 * needs no assets to run, so setting this as the default game mode is enough to
 * press play and fly.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Wind Flier Game Mode"))
class AWindFlierGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	AWindFlierGameMode();
};
