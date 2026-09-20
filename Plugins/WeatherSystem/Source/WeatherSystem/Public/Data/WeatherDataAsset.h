// Copyright TADemo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "WeatherSystemTypes.h"
#include "WeatherFrame.h"
#include "WeatherDataAsset.generated.h"

class AWeatherActor;
class UWeatherParamSchema;

/**
 * Definition of a single weather (base or custom).
 *
 * Channels ("columns") are NOT defined here — they live in a shared UWeatherParamSchema
 * referenced by Schema. Every UWeatherFrame::Values in this asset has one entry per
 * schema binding, in schema order.
 *
 * Constants is applied ONCE when this weather becomes active, and never touched again by
 * the per-frame path. Use it for values that should hold for the whole weather (e.g. base
 * cloud coverage, MPC toggles that don't animate on TOD). Keys in Constants are matched
 * against the schema by ParamName; entries with no matching schema binding are ignored.
 */
UCLASS(BlueprintType)
class WEATHERSYSTEM_API UWeatherDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Hierarchical id, e.g. Weather.Base.Rain, Weather.Custom.Sandstorm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather", meta = (Categories = "Weather"))
	FGameplayTag WeatherId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	FText DisplayName;

	/** Whether the transition controller may pick this weather during RequestNextWeather. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	bool bParticipatesInAutoSwitch = true;

	/** Shared column definitions. Frames must have Values.Num() == Schema->Bindings.Num(). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	TObjectPtr<UWeatherParamSchema> Schema;

	/**
	 * One-shot values written when this weather becomes active. Keyed by ParamName;
	 * only entries whose name matches a schema binding are used, and the schema binding
	 * decides the target (Actor property, MID, MPC, Niagara, function).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	TMap<FName, FWeatherParamValue> Constants;

	/** Time-ordered frames; Values.Num() must equal Schema->Bindings.Num() for each frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	TArray<FWeatherFrame> Frames;

	/** The single shared host Actor class for weathers (see AWeatherActor). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	TSubclassOf<AWeatherActor> HostActorClass;

	//~ Begin UObject
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("Weather"), GetFName());
	}

	/**
	 * Convenience: builds a Values array (parallel to Schema->Bindings) filled with
	 * the entries from Constants (0-typed where absent). Used by the subsystem when
	 * this weather is first bound to the host actor.
	 */
	void BuildConstantValues(TArray<FWeatherParamValue>& Out) const;

	/** Rebuild every FWeatherFrame's ResolvedValues against the current Schema. */
	void ResolveFrames();

private:
	/** Sort Frames by TimeOfDay and resolve each frame against Schema. */
	void NormalizeFrames();
};
