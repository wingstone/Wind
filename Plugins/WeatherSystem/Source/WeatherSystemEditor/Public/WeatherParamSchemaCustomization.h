// Copyright TADemo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "WeatherSystemTypes.h"

class AActor;
class FMenuBuilder;
class IDetailLayoutBuilder;
class IPropertyHandle;
class UActorComponent;
class UWeatherParamSchema;
class SWidget;

/**
 * Customization for UWeatherParamSchema.
 *
 * Replaces the flat "Bindings" array with group sections driven by FWeatherParamBinding::Group:
 *   - each group gets its own header row (name + count) with a "+ Add Binding" button that inserts
 *     a new binding inside that group;
 *   - binding rows keep the standard array-item chrome (drag / reorder / delete) and their
 *     TitleProperty name, so entries are identifiable at a glance;
 *   - editing a binding's Group field re-sorts the view immediately.
 *
 * The "+ Add Binding" button opens a Sequencer-style cascading menu so bindings can be authored
 * from existing targets instead of typed by hand:
 *     target (MPC or level actor) -> component -> material parameter / light function / property,
 * which auto-fills Target, Type, ComponentTag and ParamName on the new binding.
 *
 * The underlying Bindings array order is preserved (it defines frame row layout); grouping is
 * purely a visual editor organization.
 */
class FWeatherParamSchemaCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	TWeakPtr<class IPropertyUtilities> PropertyUtilities;
	TWeakObjectPtr<class UWeatherParamSchema> Schema;
	TSharedPtr<IPropertyHandle> BindingsHandle;

	// ----- Add-binding menu -----
	/** Root menu for the "+ Add Binding" button of one group. */
	TSharedRef<SWidget> BuildAddBindingMenuWidget(FName Group);
	void BuildMPCMenu(FMenuBuilder& MenuBuilder, FName Group);
	void BuildActorsMenu(FMenuBuilder& MenuBuilder, FName Group);
	void BuildActorComponentsMenu(FMenuBuilder& MenuBuilder, FName Group, TWeakObjectPtr<AActor> ActorPtr);
	void BuildComponentMenu(FMenuBuilder& MenuBuilder, FName Group, UActorComponent* Component);

	/** Append/insert a new binding (template fields copied) into the given group; returns the new array index (INDEX_NONE on failure). */
	int32 AddBindingWithTemplate(FName GroupName, const FWeatherParamBinding& Template);
	/** Add an empty binding with a unique auto-generated ParamName. */
	void AddBlankBinding(FName GroupName);
	/** Pick a ParamName ("Param", "Param_1", ...) that does not collide with existing bindings. */
	FName GenerateUniqueParamName() const;
};
