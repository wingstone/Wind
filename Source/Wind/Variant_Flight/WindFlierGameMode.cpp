// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFlierGameMode.h"

#include "WindFlierHUD.h"
#include "WindFlierPawn.h"

AWindFlierGameMode::AWindFlierGameMode()
{
	DefaultPawnClass = AWindFlierPawn::StaticClass();
	HUDClass = AWindFlierHUD::StaticClass();
}
