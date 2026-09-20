// Copyright TADemo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Materials/MaterialParameterCollection.h"
#include "WeatherSystemTypes.generated.h"

/** Four seasons. */
UENUM(BlueprintType)
enum class EWeatherSeason : uint8
{
	Spring,
	Summer,
	Autumn,
	Winter
};

/** Value type carried by a parameter channel. */
UENUM(BlueprintType)
enum class EWeatherParamType : uint8
{
	Float,
	Color,
	LinearColor
};

/** Where a parameter value is written. */
UENUM(BlueprintType)
enum class EWeatherParamTarget : uint8
{
	ActorProperty,       // reflect + set on a UPROPERTY of the host actor / component
	MaterialParameter,   // SetScalar/VectorParameterValue on a MID — on a mesh component, or on a light component's LightFunctionMaterial
	MPCParameter,        // Material Parameter Collection global
	NiagaraParameter,    // Niagara user parameter
	ActorFunction        // ProcessEvent on a BlueprintCallable UFunction (single arg)
};

/**
 * Tagged value carried in a frame's row. Kept as a POD-ish struct so entire rows
 * live contiguously in TArrays. Only the field matching Type is read.
 */
USTRUCT(BlueprintType)
struct WEATHERSYSTEM_API FWeatherParamValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	EWeatherParamType Type = EWeatherParamType::Float;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	float FloatValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	FLinearColor LinearColorValue = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	FColor ColorValue = FColor::White;

	/** Component-wise linear interpolation according to Type. */
	static FWeatherParamValue Lerp(const FWeatherParamValue& A, const FWeatherParamValue& B, float T);
};

/**
 * A "column" definition: describes one parameter channel — its name, value type,
 * and where the value should be written. Data assets carry ONE array of these
 * (Bindings), and every frame carries a Values array of exactly the same length.
 */
USTRUCT(BlueprintType, meta = (TitleProperty = "{DisplayName} ({ParamName})", ToolTip = "A weather parameter channel: where the value is written, its type, and its default."))
struct WEATHERSYSTEM_API FWeatherParamBinding
{
	GENERATED_BODY()

	/** Human-facing channel name; also used as the property / material param / Niagara var / function name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	FName ParamName;

	/** Optional human-friendly label; used as the entry title in editor details when set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	FText DisplayName;

	/** Where this channel's value is written (actor property / material / MPC / Niagara / function). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	EWeatherParamTarget Target = EWeatherParamTarget::MaterialParameter;

	/** Value type. Only the matching Default fields below are shown in the editor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	EWeatherParamType Type = EWeatherParamType::Float;

	/**
	 * Which component on the host actor to talk to (matched by ComponentTag or object name;
	 * NAME_None means "the first component of the appropriate type").
	 *
	 * Used by ActorProperty / ActorFunction / MaterialParameter / NiagaraParameter.
	 * Not used by MPCParameter — the schema's single MPC is always used.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather",
		meta = (EditCondition = "Target != EWeatherParamTarget::MPCParameter", EditConditionHides))
	FName ComponentTag;

	/**
	 * Logical group this channel belongs to — used only to organize the schema's Bindings list in
	 * the editor (e.g. "Sky", "Sun", "Wind", "Fog"). NAME_None places it in the "General" group.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	FName Group;

	// -------- Defaults & range ---------
	// Used when a frame does not carry an override for this channel, and (for Float)
	// to clamp resolved values into a sane range. Only the fields matching Type are shown.

	/** Default float used when Type == Float and a frame does not override this channel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Default",
		meta = (EditCondition = "Type == EWeatherParamType::Float", EditConditionHides))
	float FloatDefault = 0.0f;

	/** Minimum float — informational, drives editor slider range only. Does NOT clamp values at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Default",
		meta = (EditCondition = "Type == EWeatherParamType::Float", EditConditionHides))
	float FloatMin = 0.0f;

	/** Maximum float — informational, drives editor slider range only. Does NOT clamp values at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Default",
		meta = (EditCondition = "Type == EWeatherParamType::Float", EditConditionHides))
	float FloatMax = 1.0f;

	/** Default FColor used when Type == Color and a frame does not override this channel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Default",
		meta = (EditCondition = "Type == EWeatherParamType::Color", EditConditionHides))
	FColor ColorDefault = FColor::White;

	/** Default FLinearColor used when Type == LinearColor and a frame does not override this channel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Default",
		meta = (EditCondition = "Type == EWeatherParamType::LinearColor", EditConditionHides))
	FLinearColor LinearColorDefault = FLinearColor::White;

	/** Build a FWeatherParamValue populated with the binding's default according to Type. */
	FWeatherParamValue MakeDefaultValue() const
	{
		FWeatherParamValue V;
		V.Type = Type;
		V.FloatValue = FloatDefault;
		V.ColorValue = ColorDefault;
		V.LinearColorValue = LinearColorDefault;
		return V;
	}
};
