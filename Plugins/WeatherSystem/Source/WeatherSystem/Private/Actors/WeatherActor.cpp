// Copyright TADemo. All Rights Reserved.

#include "Actors/WeatherActor.h"

#include "Components/ExponentialHeightFogComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "Subsystems/WeatherSubsystem.h"
#if WITH_EDITOR
#include "UObject/ObjectSaveContext.h"
#endif

AWeatherActor::AWeatherActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	SkyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SkyMesh"));
	SkyMesh->SetupAttachment(Root);
	SkyMesh->ComponentTags.Add(TEXT("SkyMesh"));
	SkyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkyMesh->SetCastShadow(false);
	SkyMesh->bCastDynamicShadow = false;
	SkyMesh->SetRelativeScale3D(FVector(400.0f)); // typical skydome scale
	SkyMesh->SetMobility(EComponentMobility::Movable);

	SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
	SkyAtmosphere->SetupAttachment(Root);
	SkyAtmosphere->ComponentTags.Add(TEXT("SkyAtmosphere"));

	HeightFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("HeightFog"));
	HeightFog->SetupAttachment(Root);
	HeightFog->ComponentTags.Add(TEXT("HeightFog"));

	VolumetricCloud = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("VolumetricCloud"));
	VolumetricCloud->SetupAttachment(Root);
	VolumetricCloud->ComponentTags.Add(TEXT("VolumetricCloud"));

	SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
	SunLight->SetupAttachment(Root);
	SunLight->ComponentTags.Add(TEXT("SunLight"));
	SunLight->SetMobility(EComponentMobility::Movable);
	SunLight->SetAtmosphereSunLight(true);
	SunLight->SetAtmosphereSunLightIndex(0);
	SunLight->SetForwardShadingPriority(1);
	SunLight->SetRelativeRotation(FRotator(-45.0f, 45.0f, 0.0f));

	MoonLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("MoonLight"));
	MoonLight->SetupAttachment(Root);
	MoonLight->ComponentTags.Add(TEXT("MoonLight"));
	MoonLight->SetMobility(EComponentMobility::Movable);
	MoonLight->SetAtmosphereSunLight(true);
	MoonLight->SetAtmosphereSunLightIndex(1);
	MoonLight->SetForwardShadingPriority(0);
	MoonLight->SetIntensity(0.2f);
	MoonLight->SetRelativeRotation(FRotator(45.0f, -135.0f, 0.0f));

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->ComponentTags.Add(TEXT("SkyLight"));
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;

	RainFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("RainFX"));
	RainFX->SetupAttachment(Root);
	RainFX->ComponentTags.Add(TEXT("RainFX"));
	RainFX->SetAutoActivate(false);
	RainFX->bAutoManageAttachment = false;

	SnowFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SnowFX"));
	SnowFX->SetupAttachment(Root);
	SnowFX->ComponentTags.Add(TEXT("SnowFX"));
	SnowFX->SetAutoActivate(false);
	SnowFX->bAutoManageAttachment = false;
}

void AWeatherActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	if (UWorld* World = GetWorld())
	{
		if (UWeatherSubsystem* Sub = World->GetSubsystem<UWeatherSubsystem>())
		{
			Sub->SetWeatherHostActor(this);
		}
	}
}

void AWeatherActor::PostUnregisterAllComponents()
{
	if (UWorld* World = GetWorld())
	{
		if (UWeatherSubsystem* Sub = World->GetSubsystem<UWeatherSubsystem>())
		{
			Sub->SetWeatherHostActor(nullptr);
		}
	}
	Super::PostUnregisterAllComponents();
}

#if WITH_EDITOR
void AWeatherActor::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	// Walk a MID -> parent chain back to a non-MID MaterialInterface (or null).
	auto ResolveToParent = [](UMaterialInterface* In) -> UMaterialInterface*
	{
		UMaterialInterface* Cur = In;
		while (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Cur))
		{
			Cur = MID->Parent;
		}
		return Cur;
	};

	// 1) Mesh components — every material slot.
	TArray<UMeshComponent*> MeshComponents;
	GetComponents<UMeshComponent>(MeshComponents);
	for (UMeshComponent* MeshComp : MeshComponents)
	{
		if (!MeshComp)
		{
			continue;
		}
		const int32 NumMaterials = MeshComp->GetNumMaterials();
		for (int32 Index = 0; Index < NumMaterials; ++Index)
		{
			UMaterialInterface* Current = MeshComp->GetMaterial(Index);
			UMaterialInterface* Resolved = ResolveToParent(Current);
			if (Resolved != Current)
			{
				MeshComp->SetMaterial(Index, Resolved);
			}
		}
	}

	// 2) Light components — LightFunctionMaterial (sun / moon / any spot/point light on the actor).
	TArray<ULightComponent*> LightComponents;
	GetComponents<ULightComponent>(LightComponents);
	for (ULightComponent* LightComp : LightComponents)
	{
		if (!LightComp)
		{
			continue;
		}
		UMaterialInterface* Current = LightComp->LightFunctionMaterial;
		UMaterialInterface* Resolved = ResolveToParent(Current);
		if (Resolved != Current)
		{
			LightComp->SetLightFunctionMaterial(Resolved);
		}
	}

	// 3) Volumetric cloud material.
	TArray<UVolumetricCloudComponent*> CloudComponents;
	GetComponents<UVolumetricCloudComponent>(CloudComponents);
	for (UVolumetricCloudComponent* CloudComp : CloudComponents)
	{
		if (!CloudComp)
		{
			continue;
		}
		UMaterialInterface* Current = CloudComp->GetMaterial();
		UMaterialInterface* Resolved = ResolveToParent(Current);
		if (Resolved != Current)
		{
			CloudComp->SetMaterial(Resolved);
		}
	}

	Super::PreSave(ObjectSaveContext);
}
#endif

namespace
{
	// Build a light rotation from the celestial body's (altitude, azimuth-from-north),
	// plus the observer's compass (world yaw of north). A directional light's forward
	// vector points where its photons travel, but by long-standing convention in this
	// actor "LightYaw" is written pointing TOWARD the body's ground-projected direction
	// (materials in the project read the light rotation that way); pitch is the light's
	// downward tilt = -altitude.
	FRotator MakeCelestialRotation(float AltitudeDeg, float AzimuthFromNorthDeg, float CompassYawDeg)
	{
		const float LightPitch = -AltitudeDeg;
		const float LightYaw   = CompassYawDeg + AzimuthFromNorthDeg;
		return FRotator(LightPitch, LightYaw, 0.0f);
	}

	// Solar altitude + azimuth (from north, clockwise) at latitude Lat for time-of-day TOD,
	// assuming an equinox declination of 0 so the model stays purely a function of Lat.
	// TOD 12 -> local solar noon (peak altitude = 90 - |Lat|).
	// TOD  6 -> east horizon.
	// TOD 18 -> west horizon.
	void SolarAngles(float TOD, float LatDeg, float& OutAltDeg, float& OutAzFromNorthDeg)
	{
		const float T = FMath::Fmod(FMath::Fmod(TOD, 24.0f) + 24.0f, 24.0f);
		const float HoursFromNoon = T - 12.0f;                  // [-12, +12]
		const float H = HoursFromNoon * (PI / 12.0f);           // hour angle, rad; 0 at noon
		const float Phi = FMath::DegreesToRadians(LatDeg);

		const float SinAlt = FMath::Cos(Phi) * FMath::Cos(H);   // δ = 0 (equinox)
		const float Alt    = FMath::Asin(FMath::Clamp(SinAlt, -1.0f, 1.0f));
		OutAltDeg = FMath::RadiansToDegrees(Alt);

		// Az from north, cw:  atan2(-sin(H), -cos(H)·sin(Phi))  (with δ=0)
		const float Az = FMath::Atan2(-FMath::Sin(H), -FMath::Cos(H) * FMath::Sin(Phi));
		OutAzFromNorthDeg = FMath::RadiansToDegrees(Az);
	}
}

void AWeatherActor::UpdateCelestialBodies(float TimeOfDay)
{
	if (SunLight)
	{
		float SunAlt, SunAz;
		SolarAngles(TimeOfDay, LatitudeDegrees, SunAlt, SunAz);
		SunLight->SetWorldRotation(MakeCelestialRotation(SunAlt, SunAz, CompassYawDegrees));

		const float Band = FMath::Max(0.001f, TwilightDegrees);
		const float SunFactor = FMath::Clamp((SunAlt + Band) / (2.0f * Band), 0.0f, 1.0f);
		SunLight->SetIntensity(SunPeakIntensity * SunFactor);
		SunLight->SetVisibility(SunFactor > 0.0f);
	}

	if (MoonLight)
	{
		float MoonAlt, MoonAz;
		const float MoonTOD = FMath::Fmod(TimeOfDay + MoonHourOffset + 24.0f, 24.0f);
		// A small latitude offset for the moon gives it its own arc so it doesn't retrace the sun.
		SolarAngles(MoonTOD, LatitudeDegrees + MoonTiltDegrees, MoonAlt, MoonAz);
		MoonLight->SetWorldRotation(MakeCelestialRotation(MoonAlt, MoonAz, CompassYawDegrees));

		const float Band = FMath::Max(0.001f, TwilightDegrees);
		const float MoonFactor = FMath::Clamp((MoonAlt + Band) / (2.0f * Band), 0.0f, 1.0f);
		MoonLight->SetIntensity(MoonPeakIntensity * MoonFactor);
		MoonLight->SetVisibility(MoonFactor > 0.0f);
	}
}
