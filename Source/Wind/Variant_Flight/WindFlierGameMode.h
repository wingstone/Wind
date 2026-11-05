// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WindFlierGameMode.generated.h"

/**
 * 独立风力飞行 3C 的 GameMode。
 *
 * 与其他变体的 GameMode（作为蓝图子类的抽象基类）不同，这个 GameMode
 * 是具体的，并且自己完成了全部装配：飞行控制器不需要任何资产即可运行，
 * 所以把它设为默认 GameMode 就可以直接按下 Play 起飞。
 */
UCLASS(Blueprintable, meta = (DisplayName = "Wind Flier Game Mode"))
class AWindFlierGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	AWindFlierGameMode();
};
