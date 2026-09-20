// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindSubsystem.h"
#include "WindFieldSourceComponent.h"
#include "WindFieldDirectionalComponent.h"
#include "WindFieldConfigComponent.h"
#include "WindSystemSettings.h"
#include "WindFieldTypes.h"
#include "WindFieldSceneExtension.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "SceneInterface.h"
#include "ScenePrivate.h"
#include "RendererInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogWindSystem, Log, All);

// ============================================================================
// USubsystem interface
// ============================================================================

UWindSubsystem::UWindSubsystem()
{
#if WITH_EDITOR
	UWindSystemSettings::OnSettingsChange.AddUObject(this, &UWindSubsystem::LoadGlobalWindFieldConfig);
#endif
}

void UWindSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UWindSystemSettings* Settings = UWindSystemSettings::Get();
	WindFieldConfig = Settings->DefaultWindFieldConfig;

	UE_LOG(LogWindSystem, Log, TEXT("WindSubsystem initialized - Resolution: %dx%dx%d, WorldExtent: %.0fx%.0fx%.0f"),
		WindFieldConfig.Resolution.X, WindFieldConfig.Resolution.Y, WindFieldConfig.Resolution.Z,
		WindFieldConfig.WorldExtent.X, WindFieldConfig.WorldExtent.Y, WindFieldConfig.WorldExtent.Z);
}

void UWindSubsystem::Deinitialize()
{
	RegisteredSources.Empty();
	RegisteredConfigComponents.Empty();
	DirectionalWindComponent = nullptr;
	Super::Deinitialize();
}

void UWindSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateScroll();
	UpdateWindField();
}

TStatId UWindSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWindSubsystem, STATGROUP_Tickables);
}

bool UWindSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game
		|| WorldType == EWorldType::PIE
		|| WorldType == EWorldType::Editor
		|| WorldType == EWorldType::GamePreview;
}

// ============================================================================
// Wind source management
// ============================================================================

void UWindSubsystem::RegisterWindSource(UWindFieldSourceComponent* Source)
{
	if (Source)
	{
		RegisteredSources.AddUnique(Source);
	}
}

void UWindSubsystem::UnregisterWindSource(UWindFieldSourceComponent* Source)
{
	RegisteredSources.Remove(Source);
}

void UWindSubsystem::RegisterDirectionalWind(UWindFieldDirectionalComponent* Component)
{
	if (Component)
	{
		DirectionalWindComponent = Component;
	}
}

void UWindSubsystem::UnregisterDirectionalWind(UWindFieldDirectionalComponent* Component)
{
	if (DirectionalWindComponent == Component)
	{
		DirectionalWindComponent = nullptr;
	}
}

void UWindSubsystem::RegisterConfigComponent(UWindFieldConfigComponent* Component)
{
	if (Component)
	{
		RegisteredConfigComponents.AddUnique(Component);
	}
}

void UWindSubsystem::UnregisterConfigComponent(UWindFieldConfigComponent* Component)
{
	RegisteredConfigComponents.Remove(Component);
}

void UWindSubsystem::ApplyWindFieldConfig()
{
	if (RegisteredConfigComponents.Num() > 0)
	{
		WindFieldConfig = RegisteredConfigComponents[0]->GetWindFieldConfig();
	}
	else
	{
		WindFieldConfig = UWindSystemSettings::Get()->DefaultWindFieldConfig;
	}

	UE_LOG(LogWindSystem, Log, TEXT("WindFieldConfig applied - Resolution: %dx%dx%d, WorldExtent: %.0fx%.0fx%.0f"),
		WindFieldConfig.Resolution.X, WindFieldConfig.Resolution.Y, WindFieldConfig.Resolution.Z,
		WindFieldConfig.WorldExtent.X, WindFieldConfig.WorldExtent.Y, WindFieldConfig.WorldExtent.Z);
}

void UWindSubsystem::LoadGlobalWindFieldConfig(const UWindSystemSettings* Settings, EPropertyChangeType::Type ChangeType)
{
	// Only apply project settings if there is no per-level override
	if (RegisteredConfigComponents.Num() == 0)
	{
		WindFieldConfig = Settings->DefaultWindFieldConfig;

		UE_LOG(LogWindSystem, Log, TEXT("WindFieldConfig loaded from Project Settings"));
	}
}

FWindFieldSceneExtension* UWindSubsystem::GetSceneExtension() const
{
	if (GetWorld() && GetWorld()->Scene && GetWorld()->Scene->GetRenderScene())
	{
		return GetWorld()->Scene->GetRenderScene()->GetExtensionPtr<FWindFieldSceneExtension>();
	}
	return nullptr;
}

// ============================================================================
// Scroll — quantize camera movement and push texel offset to render thread
// ============================================================================

void UWindSubsystem::UpdateScroll()
{
	if (!GetWorld() || !GetWorld()->Scene)
	{
		return;
	}

	const FVector ScrollTargetLocation = GetFieldCenterPosition();
	const FIntVector ScrollTargetLocationInt = FIntVector(
		FMath::RoundToInt(ScrollTargetLocation.X),
		FMath::RoundToInt(ScrollTargetLocation.Y),
		FMath::RoundToInt(ScrollTargetLocation.Z));

	// Seed the scroll baseline on the first frame we get a valid, non-zero
	// target so we do not treat the player's spawn distance from the world
	// origin as a scroll delta. Without this the first non-zero tick emits a
	// scroll offset of ~PlayerLocation and pushes the volume off by that much.
	if (!bScrollBaselineInitialized)
	{
		if (!ScrollTargetLocation.IsZero())
		{
			LastScrollTargetLocation = ScrollTargetLocationInt;
			bScrollBaselineInitialized = true;
		}
		return;
	}

	const FIntVector ViewDelta = ScrollTargetLocationInt - LastScrollTargetLocation;
	const int32 MaxOffset = FMath::Max3(
		FMath::Abs(ViewDelta.X),
		FMath::Abs(ViewDelta.Y),
		FMath::Abs(ViewDelta.Z));

	// Trigger scroll when camera moves more than 25% of the smallest world extent axis
	const float ScrollThreshold = FMath::Min3(
		static_cast<float>(WindFieldConfig.WorldExtent.X),
		static_cast<float>(WindFieldConfig.WorldExtent.Y),
		static_cast<float>(WindFieldConfig.WorldExtent.Z)) * 0.25f;

	if (MaxOffset > ScrollThreshold)
	{
		const FIntVector Res(
			FMath::Max(WindFieldConfig.Resolution.X, 1),
			FMath::Max(WindFieldConfig.Resolution.Y, 1),
			FMath::Max(WindFieldConfig.Resolution.Z, 1));
		const FVector CellSize(
			WindFieldConfig.WorldExtent.X / static_cast<double>(Res.X),
			WindFieldConfig.WorldExtent.Y / static_cast<double>(Res.Y),
			WindFieldConfig.WorldExtent.Z / static_cast<double>(Res.Z));

		const FIntVector GridScrollOffset = FIntVector(
			FMath::RoundToInt32(ViewDelta.X / CellSize.X),
			FMath::RoundToInt32(ViewDelta.Y / CellSize.Y),
			FMath::RoundToInt32(ViewDelta.Z / CellSize.Z));

		ENQUEUE_RENDER_COMMAND(WindField_ScrollOffset)(
			[WorldScene = GetWorld()->Scene, GridScrollOffset](FRHICommandListImmediate& RHICmdList)
			{
				if (WorldScene->GetRenderScene())
				{
					if (FWindFieldSceneExtension* Ext = WorldScene->GetRenderScene()->GetExtensionPtr<FWindFieldSceneExtension>())
					{
						Ext->SetScrollOffset_RenderThread(GridScrollOffset);
					}
				}
			});

		LastScrollTargetLocation = ScrollTargetLocationInt;
	}
}

// ============================================================================
// GPU data push — collect sources and push to render thread
// ============================================================================

void UWindSubsystem::UpdateWindField()
{
	if (!GetWorld() || !GetWorld()->Scene)
	{
		return;
	}

	// --- Diagnostic heartbeat (once per second) ---
	{
		static double LastLogTime = 0.0;
		const double Now = FPlatformTime::Seconds();
		if (Now - LastLogTime > 1.0)
		{
			int32 ActiveCount = 0;
			for (const UWindFieldSourceComponent* S : RegisteredSources)
			{
				if (S && S->IsActive()) { ++ActiveCount; }
			}
			LastLogTime = Now;
		}
	}

	// Collect directional wind data (static, separate from fluid sources)
	FWindDirectionalData DirectionalData;
	if (DirectionalWindComponent && DirectionalWindComponent->IsWindEnabled())
	{
		DirectionalData = DirectionalWindComponent->GetDirectionalData();
	}

	// Collect fluid source data (Point, Vortex)
	TArray<FGPUWindSourceData> Sources;
	Sources.Reserve(RegisteredSources.Num());

	for (const UWindFieldSourceComponent* Source : RegisteredSources)
	{
		if (Source && Source->IsActive())
		{
			Sources.Add(Source->ToGPUData());
		}
	}

	if (Sources.Num() == 0 && !DirectionalData.IsValid())
	{
		return;
	}

	const FVector3f Center = FVector3f(GetFieldCenterPosition());
	const float Time = GetWorld()->GetTimeSeconds();
	const float Delta = GetWorld()->GetDeltaSeconds();
	const FWindFieldConfig Config = WindFieldConfig;

	ENQUEUE_RENDER_COMMAND(WindField_UpdateSources)(
		[WorldScene = GetWorld()->Scene, Sources = MoveTemp(Sources), Center, Time, Delta, Config, DirectionalData = MoveTemp(DirectionalData)](FRHICommandListImmediate& RHICmdList)
		{
			if (WorldScene->GetRenderScene())
			{
				if (FWindFieldSceneExtension* Ext = WorldScene->GetRenderScene()->GetExtensionPtr<FWindFieldSceneExtension>())
				{
					Ext->SetSourceData_RenderThread(Sources, Center, Time, Delta, Config, DirectionalData);
				}
			}
		});
}

FVector UWindSubsystem::GetFieldCenterPosition() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			if (const APawn* Pawn = PC->GetPawn())
			{
				return Pawn->GetActorLocation();
			}
			if (PC->PlayerCameraManager)
			{
				return PC->PlayerCameraManager->GetCameraLocation();
			}
		}

		auto ViewLocations = World->ViewLocationsRenderedLastFrame;
		if (ViewLocations.Num() > 0)
		{
			return ViewLocations[0];
		}
	}
	return FVector::ZeroVector;
}

// ============================================================================
// ============================================================================
// CPU sampling (fallback for AI, character movement, etc.)
// ============================================================================

FWindSample UWindSubsystem::SampleWindAtLocation(FVector WorldPosition) const
{
	FWindSample Result;

	// Global directional wind (static)
	if (DirectionalWindComponent && DirectionalWindComponent->IsWindEnabled())
	{
		Result.WindVelocity += FVector(DirectionalWindComponent->GetForwardVector()) * DirectionalWindComponent->Strength;
	}

	for (const UWindFieldSourceComponent* Source : RegisteredSources)
	{
		if (!Source || !Source->IsActive())
		{
			continue;
		}

		const FGPUWindSourceData GPUData = Source->ToGPUData();
		FVector3f Pos = FVector3f(WorldPosition);

		switch (static_cast<EWindFieldSourceType>(GPUData.WindType))
		{
		case EWindFieldSourceType::Point:
		{
			FVector3f Delta = Pos - GPUData.Position;
			float Distance = Delta.Length();
			if (Distance < GPUData.Radius && Distance > KINDA_SMALL_NUMBER)
			{
				FVector3f Dir = Delta / Distance;
				float NormDist = FMath::Clamp((Distance - GPUData.InnerRadius) / FMath::Max(GPUData.Radius - GPUData.InnerRadius, 1.0f), 0.0f, 1.0f);
				float Atten = FMath::Pow(1.0f - NormDist, GPUData.FalloffExponent);
				Result.WindVelocity += FVector(Dir * GPUData.Strength * Atten);
			}
			break;
		}
		case EWindFieldSourceType::Vortex:
		{
			FVector3f Delta = Pos - GPUData.Position;
			float HorizDist = FMath::Sqrt(Delta.X * Delta.X + Delta.Z * Delta.Z);
			if (HorizDist < GPUData.Radius && HorizDist > KINDA_SMALL_NUMBER)
			{
				FVector3f Tangent(-Delta.Z / HorizDist, 0.0f, Delta.X / HorizDist);
				float OuterFade = 1.0f - FMath::Clamp(HorizDist / GPUData.Radius, 0.0f, 1.0f);
				float InnerFade = FMath::Clamp(HorizDist / FMath::Max(GPUData.InnerRadius, 1.0f), 0.0f, 1.0f);
				Result.WindVelocity += FVector(Tangent * GPUData.Strength * OuterFade * InnerFade);
			}
			break;
		}
		case EWindFieldSourceType::Cylinder:
		{
			FVector3f Delta = Pos - GPUData.Position;
			FVector3f Axis = GPUData.Direction;
			float AxisDist = FVector3f::DotProduct(Delta, Axis);
			float HH = GPUData.HalfHeight;
			if (FMath::Abs(AxisDist) <= HH)
			{
				float t = (AxisDist + HH) / (2.0f * HH);
				float LocalRadius = FMath::Lerp(GPUData.Radius, GPUData.EndRadius, t);
				FVector3f RadialVec = Delta - Axis * AxisDist;
				float RadialDist = RadialVec.Length();
				if (RadialDist <= LocalRadius)
				{
					float RadialAtten = 1.0f;
					float InnerR = FMath::Min(GPUData.InnerRadius, LocalRadius);
					if (LocalRadius > InnerR)
					{
						float NormDist = FMath::Clamp((RadialDist - InnerR) / (LocalRadius - InnerR), 0.0f, 1.0f);
						RadialAtten = FMath::Pow(1.0f - NormDist, GPUData.FalloffExponent);
					}
					float EdgeFade = 1.0f - FMath::SmoothStep(0.85f, 1.0f, FMath::Abs(AxisDist) / HH);
					Result.WindVelocity += FVector(Axis * GPUData.Strength * RadialAtten * EdgeFade);
				}
			}
			break;
		}
		case EWindFieldSourceType::CapsuleInteractive:
		{
			const FVector3f Axis = GPUData.Direction.GetSafeNormal();
			const float R = FMath::Max(GPUData.Radius, 1.0f);
			const float HH = FMath::Max(GPUData.HalfHeight, R);
			const float SegmentHalfLen = FMath::Max(HH - R, 0.0f);

			const FVector3f Delta = Pos - GPUData.Position;
			const float AxisT = FMath::Clamp(FVector3f::DotProduct(Delta, Axis), -SegmentHalfLen, SegmentHalfLen);
			const FVector3f Closest = GPUData.Position + Axis * AxisT;
			const FVector3f ToSurface = Pos - Closest;
			const float Dist = ToSurface.Length();

			if (Dist <= R)
			{
				const float Atten = FMath::Pow(FMath::Clamp(1.0f - Dist / R, 0.0f, 1.0f), FMath::Max(GPUData.Padding, 0.001f));
				const FVector3f LinearVel(GPUData.Strength, GPUData.InnerRadius, GPUData.FalloffExponent);
				const FVector3f AngularVel(GPUData.EndRadius, GPUData.Padding3, GPUData.Padding4);
				const FVector3f LocalVel = LinearVel + FVector3f::CrossProduct(AngularVel, Pos - GPUData.Position);
				Result.WindVelocity += FVector(LocalVel * Atten);
			}

			break;
		}
		default:
			break;
		}
	}

	return Result;
}
