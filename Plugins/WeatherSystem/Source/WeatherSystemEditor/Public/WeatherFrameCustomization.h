// Copyright TADemo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

class UWeatherParamSchema;
struct FWeatherParamBinding;

/**
 * Customization for FWeatherFrame.
 *
 * Replaces the default TMap<FName, FWeatherParamValue> "Overrides" display with:
 *   - a curated list of existing entries, each shown with its schema binding's DisplayName,
 *     the appropriate value editor (slider for Float, color picker for Color/LinearColor),
 *     and a delete button;
 *   - an "Add Override" combo button whose menu enumerates schema keys not yet in the map.
 *
 * The TimeOfDay field keeps its default row.
 */
class FWeatherFrameCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	//~ IPropertyTypeCustomization
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle,
	                             class FDetailWidgetRow& HeaderRow,
	                             IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle,
	                               class IDetailChildrenBuilder& ChildBuilder,
	                               IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	TSharedPtr<IPropertyHandle> StructHandle;
	TSharedPtr<IPropertyHandle> OverridesHandle;
	TWeakPtr<class IPropertyUtilities> PropertyUtilities;

	/** Resolve the schema associated with this frame's owning asset. Returns nullptr if unavailable. */
	UWeatherParamSchema* ResolveSchema() const;

	/** Rebuild the "Add..." menu based on missing schema entries. */
	TSharedRef<class SWidget> BuildAddMenu();

	/** Insert a new entry keyed on ParamName with a default value from the schema binding. */
	void AddOverride(FName ParamName);
	/** Remove an entry by key. */
	void RemoveOverride(FName ParamName);
};
