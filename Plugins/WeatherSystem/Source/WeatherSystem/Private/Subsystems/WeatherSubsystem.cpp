// Copyright TADemo. All Rights Reserved.

#include "Subsystems/WeatherSubsystem.h"

#include "Actors/LocalWeatherVolume.h"
#include "Actors/SeasonActor.h"
#include "Actors/WeatherActor.h"
#include "Actors/WeatherAddonActor.h"
#include "Controllers/WeatherTransitionController.h"
#include "Data/SeasonDataAsset.h"
#include "Data/WeatherAddonDataAsset.h"
#include "Data/WeatherDataAsset.h"
#include "Data/WeatherParamSchema.h"
#include "Data/WeatherSystemConfig.h"
#include "Engine/World.h"
#include "WeatherFrame.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeatherSubsystem, Log, All);

// ----------------------------------------------------------------------------------------
// Initialization
// ----------------------------------------------------------------------------------------

void UWeatherSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Controller = NewObject<UWeatherTransitionController>(this);
	Rng.Initialize(FGuid::NewGuid().A);
}

void UWeatherSubsystem::Deinitialize()
{
	// We don't own the addon actors (users placed them); just clear our references.
	ActiveAddons.Reset();
	RegisteredAddonActors.Reset();
	PendingAddonToggles.Reset();
	Super::Deinitialize();
}

// ----------------------------------------------------------------------------------------
// Config / registry
// ----------------------------------------------------------------------------------------

void UWeatherSubsystem::SetConfig(UWeatherSystemConfig* InConfig)
{
	Config = InConfig;
	WeatherRegistry.Reset();
	SeasonRegistry.Reset();
	AddonRegistry.Reset();
	if (!Config) return;

	for (const TSoftObjectPtr<UWeatherDataAsset>& S : Config->Weathers)
	{
		if (UWeatherDataAsset* W = S.LoadSynchronous())
		{
			W->ResolveFrames();
			if (W->WeatherId.IsValid()) WeatherRegistry.Add(W->WeatherId, W);
		}
	}
	for (const TSoftObjectPtr<USeasonDataAsset>& S : Config->Seasons)
	{
		if (USeasonDataAsset* Sd = S.LoadSynchronous())
		{
			Sd->ResolveFrames();
			SeasonRegistry.Add(Sd->Season, Sd);
		}
	}
	for (const TSoftObjectPtr<UWeatherAddonDataAsset>& S : Config->Addons)
	{
		if (UWeatherAddonDataAsset* A = S.LoadSynchronous())
		{
			A->ResolveFrames();
			if (A->AddonId.IsValid()) AddonRegistry.Add(A->AddonId, A);
		}
	}

	// Addon actors may have registered before the config was applied (registry empty then).
	// Retry binding so every placed actor that now has a resolvable asset gets its slot.
	for (const TWeakObjectPtr<AWeatherAddonActor>& Weak : RegisteredAddonActors)
	{
		if (AWeatherAddonActor* Actor = Weak.Get())
		{
			TryBindAddonActor(Actor);
		}
	}
}

UWeatherDataAsset* UWeatherSubsystem::FindWeather(FGameplayTag Id) const
{
	if (const TObjectPtr<UWeatherDataAsset>* Found = WeatherRegistry.Find(Id)) return *Found;
	return nullptr;
}

USeasonDataAsset* UWeatherSubsystem::FindSeason(EWeatherSeason Season) const
{
	if (const TObjectPtr<USeasonDataAsset>* Found = SeasonRegistry.Find(Season)) return *Found;
	return nullptr;
}

UWeatherAddonDataAsset* UWeatherSubsystem::FindAddon(FGameplayTag Id) const
{
	if (const TObjectPtr<UWeatherAddonDataAsset>* Found = AddonRegistry.Find(Id)) return *Found;
	return nullptr;
}

// ----------------------------------------------------------------------------------------
// Host actors
// ----------------------------------------------------------------------------------------

void UWeatherSubsystem::SetWeatherHostActor(AWeatherActor* InActor)
{
	WeatherHost = InActor;
	ResolveWeatherAppliers();
}

void UWeatherSubsystem::SetSeasonHostActor(ASeasonActor* InActor)
{
	SeasonHost = InActor;
	ResolveSeasonAppliers();
}

void UWeatherSubsystem::ResolveSchemaDefaults()
{
	SchemaDefaultsApplier.Reset();
	SchemaDefaultsValues.Reset();
	if (!WeatherHost || !Config || !Config->Schema) return;

	SchemaDefaultsApplier.ResolveBindings(WeatherHost, Config->Schema);

	const TArray<FWeatherParamBinding>& Bindings = Config->Schema->Bindings;
	const int32 N = Bindings.Num();
	SchemaDefaultsValues.SetNum(N);
	for (int32 i = 0; i < N; ++i)
	{
		SchemaDefaultsValues[i] = Bindings[i].MakeDefaultValue();
	}
}

void UWeatherSubsystem::ResolveWeatherAppliers()
{
	if (!WeatherHost) return;
	ResolveSchemaDefaults();

	if (CurrentWeather && CurrentWeather->Schema)
	{
		CurrentWeatherApplier.ResolveBindings(WeatherHost, CurrentWeather->Schema);
		TArray<FWeatherParamValue> Const;
		CurrentWeather->BuildConstantValues(Const);
		if (Const.Num() > 0) CurrentWeatherApplier.Apply(Const, GetWorld());
	}
	else
	{
		CurrentWeatherApplier.Reset();
	}

	if (PrevWeather && PrevWeather->Schema)
	{
		PrevWeatherApplier.ResolveBindings(WeatherHost, PrevWeather->Schema);
	}
	else
	{
		PrevWeatherApplier.Reset();
	}
}

void UWeatherSubsystem::ResolveSeasonAppliers()
{
	if (!SeasonHost) return;

	if (CurrentSeasonAsset) CurrentSeasonApplier.ResolveBindingsRaw(SeasonHost, CurrentSeasonAsset->Bindings);
	else                    CurrentSeasonApplier.Reset();

	if (PrevSeasonAsset) PrevSeasonApplier.ResolveBindingsRaw(SeasonHost, PrevSeasonAsset->Bindings);
	else                 PrevSeasonApplier.Reset();
}

// ----------------------------------------------------------------------------------------
// Setters
// ----------------------------------------------------------------------------------------

float UWeatherSubsystem::ResolveBlendSeconds(float Requested, float Default) const
{
	if (Requested >= 0.0f) return Requested;
	return Default;
}

void UWeatherSubsystem::SetTimeOfDay(float InTOD)
{
	CurrentTOD = FMath::Fmod(FMath::Max(0.0f, InTOD), 24.0f);
}

void UWeatherSubsystem::SetSeasonProgress(float Phase)
{
	CurrentSeasonPhase = FMath::Clamp(Phase, 0.0f, 2.0f);
}

void UWeatherSubsystem::SetWeather(FGameplayTag WeatherId, float BlendSeconds)
{
	UWeatherDataAsset* NewAsset = FindWeather(WeatherId);
	if (!NewAsset)
	{
		UE_LOG(LogWeatherSubsystem, Warning, TEXT("SetWeather: unknown id %s"), *WeatherId.ToString());
		return;
	}
	if (NewAsset == CurrentWeather) return;

	const float Blend = ResolveBlendSeconds(BlendSeconds, Config ? Config->DefaultWeatherBlendSeconds : 5.0f);
	const FGameplayTag OldId = CurrentWeatherId;

	PrevWeather   = CurrentWeather;
	PrevWeatherId = CurrentWeatherId;
	CurrentWeather   = NewAsset;
	CurrentWeatherId = WeatherId;

	if (Blend <= 0.0f)
	{
		WeatherAlpha = 1.0f;
		WeatherRate  = 0.0f;
	}
	else
	{
		WeatherAlpha = 0.0f;
		WeatherRate  = 1.0f / Blend;
	}

	ResolveWeatherAppliers();
	OnWeatherChanged.Broadcast(OldId, CurrentWeatherId);
}

void UWeatherSubsystem::SetSeason(EWeatherSeason Season, float BlendSeconds)
{
	USeasonDataAsset* NewAsset = FindSeason(Season);
	if (!NewAsset)
	{
		UE_LOG(LogWeatherSubsystem, Warning, TEXT("SetSeason: no asset for season %d"), (int32)Season);
		return;
	}
	if (NewAsset == CurrentSeasonAsset) return;

	const float Blend = ResolveBlendSeconds(BlendSeconds, Config ? Config->DefaultSeasonBlendSeconds : 30.0f);
	const EWeatherSeason OldSeason = CurrentSeason;

	PrevSeasonAsset = CurrentSeasonAsset;
	PrevSeason      = CurrentSeason;
	CurrentSeasonAsset = NewAsset;
	CurrentSeason      = Season;

	if (Blend <= 0.0f)
	{
		SeasonAlpha = 1.0f;
		SeasonRate  = 0.0f;
	}
	else
	{
		SeasonAlpha = 0.0f;
		SeasonRate  = 1.0f / Blend;
	}

	ResolveSeasonAppliers();
	OnSeasonChanged.Broadcast(OldSeason, CurrentSeason);
	// Per requirements: season change never triggers a weather change on its own.
}

void UWeatherSubsystem::RequestNextWeather(float BlendSeconds)
{
	if (!Controller) return;
	const UDataTable* Table = (Config && !Config->TransitionTable.IsNull())
		? Config->TransitionTable.LoadSynchronous()
		: nullptr;

	FGameplayTag Next;
	if (!Controller->PickNext(CurrentWeatherId, CurrentSeason, Table, CurrentSeasonAsset, Rng, Next))
	{
		UE_LOG(LogWeatherSubsystem, Verbose, TEXT("RequestNextWeather: no candidate."));
		return;
	}
	// Respect bParticipatesInAutoSwitch on the target.
	if (UWeatherDataAsset* Target = FindWeather(Next))
	{
		if (!Target->bParticipatesInAutoSwitch)
		{
			UE_LOG(LogWeatherSubsystem, Verbose, TEXT("RequestNextWeather: %s is opted out of auto-switch."), *Next.ToString());
			return;
		}
	}
	SetWeather(Next, BlendSeconds);
}

void UWeatherSubsystem::RegisterAddonActor(AWeatherAddonActor* Actor)
{
	if (!Actor) return;
	RegisteredAddonActors.AddUnique(Actor);
	if (TryBindAddonActor(Actor))
	{
		// Apply any enable/disable request that arrived before this actor registered.
		if (const bool* Pending = PendingAddonToggles.Find(Actor->AddonId))
		{
			PendingAddonToggles.Remove(Actor->AddonId);
			EnableAddon(Actor->AddonId, *Pending);
		}
	}
}

void UWeatherSubsystem::UnregisterAddonActor(AWeatherAddonActor* Actor)
{
	if (!Actor) return;
	RegisteredAddonActors.RemoveAll([&](const TWeakObjectPtr<AWeatherAddonActor>& W) { return !W.IsValid() || W.Get() == Actor; });
	// Drop its active slot; the placed actor simply stops being driven.
	ActiveAddons.RemoveAll([&](const FActiveAddon& A) { return A.Actor.Get() == Actor; });
}

bool UWeatherSubsystem::TryBindAddonActor(AWeatherAddonActor* Actor)
{
	if (!Actor) return false;

	// Already bound to the same id? Nothing to do.
	FActiveAddon* Existing = ActiveAddons.FindByPredicate([&](const FActiveAddon& A) { return A.Actor.Get() == Actor; });
	if (Existing && Existing->Id == Actor->AddonId && Existing->Asset.IsValid())
	{
		return true;
	}

	// Stale slot (AddonId changed) — rebuild it below.
	if (Existing)
	{
		ActiveAddons.RemoveAt(static_cast<int32>(Existing - ActiveAddons.GetData()));
	}

	UWeatherAddonDataAsset* Asset = FindAddon(Actor->AddonId);
	if (!Asset)
	{
		UE_LOG(LogWeatherSubsystem, Verbose,
			TEXT("TryBindAddonActor: no addon asset registered for id %s (config not set yet?)"), *Actor->AddonId.ToString());
		return false;
	}

	FActiveAddon Slot;
	Slot.Id = Actor->AddonId;
	Slot.Asset = Asset;
	Slot.Actor = Actor;
	Slot.bEnabled = true;   // a placed actor is on by default; EnableAddon toggles it off
	Slot.EnvelopeAlpha = 0.0f;
	Slot.EnvelopeRate  = 0.0f;
	Slot.Applier.ResolveBindings(Actor, Asset->Schema);

	Actor->AddonAsset = Asset;
	Actor->OnAddonAssetBound(Asset);

	// Apply one-shot constants immediately.
	TArray<FWeatherParamValue> Const;
	Asset->BuildConstantValues(Const);
	if (Const.Num() > 0) Slot.Applier.Apply(Const, GetWorld());

	ActiveAddons.Add(MoveTemp(Slot));
	return true;
}

void UWeatherSubsystem::EnableAddon(FGameplayTag AddonId, bool bEnable)
{
	// Lazily bind any placed-but-unbound actor for this addon id before toggling.
	for (const TWeakObjectPtr<AWeatherAddonActor>& Weak : RegisteredAddonActors)
	{
		if (AWeatherAddonActor* Actor = Weak.Get())
		{
			if (Actor->AddonId == AddonId) TryBindAddonActor(Actor);
		}
	}

	const float Blend = Config ? Config->DefaultAddonBlendSeconds : 3.0f;
	const float Rate  = (Blend > 0.0f) ? (1.0f / Blend) : 0.0f;

	int32 Toggled = 0;
	for (FActiveAddon& A : ActiveAddons)
	{
		if (A.Id == AddonId)
		{
			A.bEnabled = bEnable;
			A.EnvelopeRate = Rate;
			++Toggled;
		}
	}

	if (Toggled == 0)
	{
		// Nothing bound yet — remember the request so a later-registered actor picks it up.
		PendingAddonToggles.Add(AddonId, bEnable);

		const bool bHasPlacedActor = RegisteredAddonActors.ContainsByPredicate(
			[&](const TWeakObjectPtr<AWeatherAddonActor>& W) { return W.IsValid() && W.Get()->AddonId == AddonId; });
		if (bHasPlacedActor || FindAddon(AddonId))
		{
			// A placed actor and/or an asset exists — binding will complete soon.
			UE_LOG(LogWeatherSubsystem, Verbose,
				TEXT("EnableAddon(%s, %s): deferred until its actor binds."),
				*AddonId.ToString(), bEnable ? TEXT("true") : TEXT("false"));
		}
		else
		{
			UE_LOG(LogWeatherSubsystem, Warning,
				TEXT("EnableAddon(%s): unknown addon — no data asset in config and no AWeatherAddonActor placed in the level."),
				*AddonId.ToString());
		}
	}
}

void UWeatherSubsystem::RegisterLocalVolume(ALocalWeatherVolume* Volume)
{
	if (!Volume) return;
	LocalVolumes.AddUnique(Volume);
	LocalVolumes.Sort([](const TWeakObjectPtr<ALocalWeatherVolume>& A, const TWeakObjectPtr<ALocalWeatherVolume>& B)
	{
		const ALocalWeatherVolume* Ap = A.Get();
		const ALocalWeatherVolume* Bp = B.Get();
		if (!Ap) return false;
		if (!Bp) return true;
		return Ap->Priority > Bp->Priority;
	});
}

void UWeatherSubsystem::UnregisterLocalVolume(ALocalWeatherVolume* Volume)
{
	LocalVolumes.RemoveAll([&](const TWeakObjectPtr<ALocalWeatherVolume>& W) { return !W.IsValid() || W.Get() == Volume; });
}

// ----------------------------------------------------------------------------------------
// Sampling
// ----------------------------------------------------------------------------------------

void UWeatherSubsystem::SampleWeather(const UWeatherDataAsset* Asset, float TOD, TArray<FWeatherParamValue>& Out) const
{
	Out.Reset();
	if (!Asset || Asset->Frames.Num() == 0) return;
	int32 Lo, Hi; float A;
	WeatherFrameSampling::FindFrames<FWeatherFrame>(Asset->Frames, TOD, &FWeatherFrame::TimeOfDay, Lo, Hi, A);
	if (Lo == Hi)
	{
		Out = Asset->Frames[Lo].ResolvedValues;
		return;
	}
	WeatherFrameSampling::BlendValueArrays(Asset->Frames[Lo].ResolvedValues, Asset->Frames[Hi].ResolvedValues, A, Out);
}

void UWeatherSubsystem::SampleSeason(const USeasonDataAsset* Asset, float Phase, TArray<FWeatherParamValue>& Out) const
{
	Out.Reset();
	if (!Asset || Asset->Frames.Num() == 0) return;
	int32 Lo, Hi; float A;
	WeatherFrameSampling::FindFrames<FSeasonFrame>(Asset->Frames, Phase, &FSeasonFrame::SeasonPhase, Lo, Hi, A);
	if (Lo == Hi)
	{
		Out = Asset->Frames[Lo].ResolvedValues;
		return;
	}
	WeatherFrameSampling::BlendValueArrays(Asset->Frames[Lo].ResolvedValues, Asset->Frames[Hi].ResolvedValues, A, Out);
}

void UWeatherSubsystem::SampleAddon(const UWeatherAddonDataAsset* Asset, float TOD, float& OutWeight, TArray<FWeatherParamValue>& Out) const
{
	OutWeight = 0.0f;
	Out.Reset();
	if (!Asset || Asset->Frames.Num() == 0) return;
	int32 Lo, Hi; float A;
	WeatherFrameSampling::FindFrames<FAddonFrame>(Asset->Frames, TOD, &FAddonFrame::TimeOfDay, Lo, Hi, A);
	if (Lo == Hi)
	{
		Out = Asset->Frames[Lo].ResolvedValues;
		OutWeight = Asset->Frames[Lo].Weight;
		return;
	}
	WeatherFrameSampling::BlendValueArrays(Asset->Frames[Lo].ResolvedValues, Asset->Frames[Hi].ResolvedValues, A, Out);
	OutWeight = FMath::Lerp(Asset->Frames[Lo].Weight, Asset->Frames[Hi].Weight, A);
}

// ----------------------------------------------------------------------------------------
// Tick
// ----------------------------------------------------------------------------------------

void UWeatherSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	TickWeather(DeltaTime);
	TickSeason(DeltaTime);
	TickAddons(DeltaTime);
}

void UWeatherSubsystem::TickWeather(float Dt)
{
	// Drive celestial bodies on the host actor first so any weather-frame overrides can
	// still stomp specific properties afterwards.
	if (WeatherHost && WeatherHost->bAutoUpdateCelestials)
	{
		WeatherHost->UpdateCelestialBodies(CurrentTOD);
	}

	// Reset every schema channel to its declared default BEFORE any weather / volume /
	// prev-weather write. Guarantees no channel silently inherits a value from the last
	// active weather when the incoming weather doesn't touch it.
	if (WeatherHost && SchemaDefaultsApplier.Num() > 0 && SchemaDefaultsValues.Num() > 0)
	{
		SchemaDefaultsApplier.Apply(SchemaDefaultsValues, GetWorld());
	}

	if (WeatherAlpha < 1.0f && WeatherRate > 0.0f)
	{
		WeatherAlpha = FMath::Min(1.0f, WeatherAlpha + WeatherRate * Dt);
		if (WeatherAlpha >= 1.0f)
		{
			// blend complete
			PrevWeather = nullptr;
			PrevWeatherId = FGameplayTag();
			PrevWeatherApplier.Reset();
		}
	}

	if (!CurrentWeather || !WeatherHost)
	{
		return;
	}

	UWorld* World = GetWorld();
	SampleWeather(CurrentWeather, CurrentTOD, ScratchA);

	// Local-volume override (single dominant volume by priority + falloff weight).
	TickLocalVolumes();

	if (PrevWeather && WeatherAlpha < 1.0f)
	{
		SampleWeather(PrevWeather, CurrentTOD, ScratchB);
		// Apply previous weather values (through prev applier) then current values, so the
		// visible result on the shared host is Lerp(prev, current, alpha) by way of two writes.
		// For channels that also exist on Current, the current write below will overwrite.
		if (PrevWeatherApplier.Num() > 0)
		{
			PrevWeatherApplier.Apply(ScratchB, World);
		}
	}

	if (bHasQueryLocation && ScratchLocal.Num() == ScratchA.Num() && ScratchA.Num() > 0)
	{
		// ScratchLocal contains the volume-composited result; use it.
		CurrentWeatherApplier.Apply(ScratchLocal, World);
	}
	else if (PrevWeather && WeatherAlpha < 1.0f && ScratchA.Num() == ScratchB.Num())
	{
		CurrentWeatherApplier.ApplyBlended(ScratchB, ScratchA, WeatherAlpha, World);
	}
	else
	{
		CurrentWeatherApplier.Apply(ScratchA, World);
	}
}

void UWeatherSubsystem::TickSeason(float Dt)
{
	if (SeasonAlpha < 1.0f && SeasonRate > 0.0f)
	{
		SeasonAlpha = FMath::Min(1.0f, SeasonAlpha + SeasonRate * Dt);
		if (SeasonAlpha >= 1.0f)
		{
			PrevSeasonAsset = nullptr;
			PrevSeasonApplier.Reset();
		}
	}

	if (!CurrentSeasonAsset || !SeasonHost) return;

	UWorld* World = GetWorld();
	SampleSeason(CurrentSeasonAsset, CurrentSeasonPhase, ScratchA);

	if (PrevSeasonAsset && SeasonAlpha < 1.0f)
	{
		SampleSeason(PrevSeasonAsset, CurrentSeasonPhase, ScratchB);
		if (PrevSeasonApplier.Num() > 0)
		{
			PrevSeasonApplier.Apply(ScratchB, World);
		}
		if (ScratchA.Num() == ScratchB.Num())
		{
			CurrentSeasonApplier.ApplyBlended(ScratchB, ScratchA, SeasonAlpha, World);
			return;
		}
	}
	CurrentSeasonApplier.Apply(ScratchA, World);
}

void UWeatherSubsystem::TickAddons(float Dt)
{
	UWorld* World = GetWorld();

	for (int32 i = ActiveAddons.Num() - 1; i >= 0; --i)
	{
		FActiveAddon& A = ActiveAddons[i];

		// Actor gone without unregistering — drop the slot.
		if (!A.Actor.IsValid())
		{
			ActiveAddons.RemoveAtSwap(i);
			continue;
		}

		const float Target = A.bEnabled ? 1.0f : 0.0f;
		if (A.EnvelopeRate <= 0.0f)
		{
			A.EnvelopeAlpha = Target;
		}
		else if (!FMath::IsNearlyEqual(A.EnvelopeAlpha, Target))
		{
			const float Step = A.EnvelopeRate * Dt;
			A.EnvelopeAlpha = (Target > A.EnvelopeAlpha)
				? FMath::Min(Target, A.EnvelopeAlpha + Step)
				: FMath::Max(Target, A.EnvelopeAlpha - Step);
		}

		AWeatherAddonActor* Actor = A.Actor.Get();
		UWeatherAddonDataAsset* Asset = A.Asset.Get();

		// Fully off: the actor is user-placed and stays in the level — keep the slot for a
		// later re-enable, but stop writing values so a disabled addon can't leak state.
		if (!A.bEnabled && A.EnvelopeAlpha <= 0.0f)
		{
			if (Actor) Actor->SetAddonWeight(0.0f);
			continue;
		}

		if (!Actor || !Asset) continue;

		float FrameWeight = 0.0f;
		SampleAddon(Asset, CurrentTOD, FrameWeight, ScratchC);
		const float Effective = FrameWeight * A.EnvelopeAlpha;
		Actor->SetAddonWeight(Effective);

		if (A.Applier.Num() > 0 && ScratchC.Num() > 0)
		{
			A.Applier.Apply(ScratchC, World);
		}
	}
}

void UWeatherSubsystem::TickLocalVolumes()
{
	ScratchLocal.Reset();
	if (!bHasQueryLocation) return;
	if (LocalVolumes.Num() == 0) return;
	if (ScratchA.Num() == 0) return;

	// Start from global (already blended if PrevWeather exists).
	if (PrevWeather && WeatherAlpha < 1.0f && ScratchA.Num() == ScratchB.Num())
	{
		WeatherFrameSampling::BlendValueArrays(ScratchB, ScratchA, WeatherAlpha, ScratchLocal);
	}
	else
	{
		ScratchLocal = ScratchA;
	}

	float BudgetAlpha = 1.0f;  // remaining share for lower-priority volumes to consume

	for (const TWeakObjectPtr<ALocalWeatherVolume>& Weak : LocalVolumes)
	{
		if (BudgetAlpha <= 0.0f) break;
		ALocalWeatherVolume* V = Weak.Get();
		if (!V || !V->WeatherAsset) continue;
		const float W = V->GetInfluenceWeight(QueryLocation);
		if (W <= 0.0f) continue;

		TArray<FWeatherParamValue> Local;
		SampleWeather(V->WeatherAsset, CurrentTOD, Local);
		if (Local.Num() != ScratchLocal.Num()) continue;

		const float Effective = FMath::Clamp(W, 0.0f, 1.0f) * BudgetAlpha;
		TArray<FWeatherParamValue> Tmp;
		WeatherFrameSampling::BlendValueArrays(ScratchLocal, Local, Effective, Tmp);
		ScratchLocal = MoveTemp(Tmp);
		BudgetAlpha *= (1.0f - Effective);
	}
}
