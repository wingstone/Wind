// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFieldSourceComponent.h"
#include "WindSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogWindSource, Log, All);

UWindFieldSourceComponent::UWindFieldSourceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bAutoActivate = true;
	bTickInEditor = true;
	SetIsReplicatedByDefault(false);

#if WITH_EDITORONLY_DATA
	bVisualizeComponent = true;
#endif
}

void UWindFieldSourceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if WITH_EDITOR
	if (UWorld* World = GetWorld())
	{
		// 仅在编辑器预览世界（非 PIE / 非运行时）绘制调试
		if (World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::EditorPreview)
		{
			DrawDebug();
		}
	}
#endif
}

void UWindFieldSourceComponent::OnRegister()
{
	Super::OnRegister();
	RegisterWithSubsystem();
}

void UWindFieldSourceComponent::OnUnregister()
{
	UnregisterFromSubsystem();
	Super::OnUnregister();
}

void UWindFieldSourceComponent::RegisterWithSubsystem()
{
	UWorld* World = GetWorld();
	UE_LOG(LogWindSource, Warning, TEXT("[WindSource] Register attempt: Owner=%s Comp=%s Class=%s World=%s WorldType=%d IsActive=%d"),
		*GetNameSafe(GetOwner()),
		*GetName(),
		*GetClass()->GetName(),
		*GetNameSafe(World),
		World ? (int32)World->WorldType : -1,
		IsActive() ? 1 : 0);

	if (World)
	{
		if (UWindSubsystem* Subsystem = World->GetSubsystem<UWindSubsystem>())
		{
			Subsystem->RegisterWindSource(this);
			UE_LOG(LogWindSource, Warning, TEXT("[WindSource]   -> registered OK (Comp=%s)"), *GetName());
		}
		else
		{
			UE_LOG(LogWindSource, Warning, TEXT("[WindSource]   -> NO SUBSYSTEM (Comp=%s WorldType=%d)"),
				*GetName(), (int32)World->WorldType);
		}
	}
	else
	{
		UE_LOG(LogWindSource, Warning, TEXT("[WindSource]   -> NO WORLD (Comp=%s)"), *GetName());
	}
}

void UWindFieldSourceComponent::UnregisterFromSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UWindSubsystem* Subsystem = World->GetSubsystem<UWindSubsystem>())
		{
			Subsystem->UnregisterWindSource(this);
			UE_LOG(LogWindSource, Warning, TEXT("[WindSource] Unregistered: Owner=%s Comp=%s"),
				*GetNameSafe(GetOwner()), *GetName());
		}
	}
}
