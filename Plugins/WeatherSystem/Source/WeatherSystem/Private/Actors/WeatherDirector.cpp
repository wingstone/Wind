// Copyright TADemo. All Rights Reserved.

#include "Actors/WeatherDirector.h"

#include "Data/WeatherSystemConfig.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Subsystems/WeatherSubsystem.h"

AWeatherDirector::AWeatherDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	// Editor-preview ticking:
	PrimaryActorTick.bTickEvenWhenPaused = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

UWeatherSubsystem* AWeatherDirector::GetSubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UWeatherSubsystem>() : nullptr;
}

namespace
{
	/** Shared "push initial state → subsystem" step used from both PostRegisterAllComponents and BeginPlay. */
	void ApplyInitialState(AWeatherDirector* Self, UWeatherSubsystem* Sub)
	{
		if (!Self || !Sub) return;
		Sub->SetConfig(Self->Config);
		Sub->SetTimeOfDay(Self->InitialTimeOfDay);
		Sub->SetSeasonProgress(Self->InitialSeasonProgress);
		Sub->SetSeason(Self->InitialSeason, 0.0f);
		if (Self->InitialWeather.IsValid())
		{
			Sub->SetWeather(Self->InitialWeather, 0.0f);
		}
		for (const FGameplayTag& Id : Self->InitiallyEnabledAddons)
		{
			if (Id.IsValid()) Sub->EnableAddon(Id, true);
		}
	}
}

void AWeatherDirector::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	// In-editor placement / duplicate / property change all funnel through here — apply the
	// preview state so the level "shows" what the director will produce at PIE start.
	if (UWorld* World = GetWorld())
	{
		// Skip pre-PIE preloads / templates so we only touch real editor / game worlds.
		if (World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::EditorPreview)
		{
			ApplyInitialState(this, GetSubsystem());
		}
	}
}

void AWeatherDirector::BeginPlay()
{
	Super::BeginPlay();
	ApplyInitialState(this, GetSubsystem());
	// AWeatherActor / ASeasonActor auto-register themselves in PostRegisterAllComponents.
}

void AWeatherDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void AWeatherDirector::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (UWorld* World = GetWorld())
	{
		if (World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::EditorPreview)
		{
			ApplyInitialState(this, GetSubsystem());
		}
	}
}
#endif

void AWeatherDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWeatherSubsystem* Sub = GetSubsystem();
	if (!Sub) return;

	if (bAutoAdvanceTime && RealSecondsPerHour > 0.0f)
	{
		const float Cur = Sub->GetTimeOfDay();
		const float Next = FMath::Fmod(Cur + (DeltaSeconds / RealSecondsPerHour) + 24.0f, 24.0f);
		Sub->SetTimeOfDay(Next);
	}

	if (bAutoRequestNextWeather && AutoWeatherInterval > 0.0f)
	{
		AutoWeatherTimer += DeltaSeconds;
		if (AutoWeatherTimer >= AutoWeatherInterval)
		{
			AutoWeatherTimer = 0.0f;
			Sub->RequestNextWeather();
		}
	}

	if (bDriveQueryLocationFromCamera)
	{
		UWorld* World = GetWorld();
		if (!World) return;
#if WITH_EDITOR
		if (World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::EditorPreview)
		{
			// In-editor preview: no player camera exists, so use the director's actor position.
			// Move the director around the level to preview local weather volumes.
			Sub->SetQueryLocation(GetActorLocation());
			return;
		}
#endif
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			FVector Loc; FRotator Rot;
			PC->GetPlayerViewPoint(Loc, Rot);
			Sub->SetQueryLocation(Loc);
		}
	}
}

// ------------- Thin wrappers -----------------------------------------------

void AWeatherDirector::SetTimeOfDay(float NewTOD)             { if (auto* S = GetSubsystem()) S->SetTimeOfDay(NewTOD); }
void AWeatherDirector::AddHours(float Hours)                  { if (auto* S = GetSubsystem()) S->SetTimeOfDay(FMath::Fmod(S->GetTimeOfDay() + Hours + 24.0f, 24.0f)); }
void AWeatherDirector::SetSeasonProgress(float Phase)         { if (auto* S = GetSubsystem()) S->SetSeasonProgress(Phase); }
void AWeatherDirector::SetWeather(FGameplayTag Id, float B)   { if (auto* S = GetSubsystem()) S->SetWeather(Id, B); }
void AWeatherDirector::SetSeason(EWeatherSeason Se, float B)  { if (auto* S = GetSubsystem()) S->SetSeason(Se, B); }
void AWeatherDirector::RequestNextWeather(float B)            { if (auto* S = GetSubsystem()) S->RequestNextWeather(B); }
void AWeatherDirector::EnableAddon(FGameplayTag Id, bool bE)  { if (auto* S = GetSubsystem()) S->EnableAddon(Id, bE); }
float AWeatherDirector::GetTimeOfDay() const                  { auto* S = GetSubsystem(); return S ? S->GetTimeOfDay() : 0.0f; }
FGameplayTag AWeatherDirector::GetCurrentWeather() const      { auto* S = GetSubsystem(); return S ? S->GetCurrentWeather() : FGameplayTag(); }
EWeatherSeason AWeatherDirector::GetCurrentSeason() const     { auto* S = GetSubsystem(); return S ? S->GetCurrentSeason() : EWeatherSeason::Spring; }
