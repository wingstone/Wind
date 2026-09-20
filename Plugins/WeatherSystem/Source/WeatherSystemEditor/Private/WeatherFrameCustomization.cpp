// Copyright TADemo. All Rights Reserved.

#include "WeatherFrameCustomization.h"

#include "Data/WeatherAddonDataAsset.h"
#include "Data/WeatherDataAsset.h"
#include "Data/WeatherParamSchema.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "WeatherFrame.h"

#define LOCTEXT_NAMESPACE "WeatherFrameCustomization"

TSharedRef<IPropertyTypeCustomization> FWeatherFrameCustomization::MakeInstance()
{
	return MakeShared<FWeatherFrameCustomization>();
}

// -----------------------------------------------------------------------------------
// Helpers to reach the map and the schema
// -----------------------------------------------------------------------------------

namespace
{
	/** Return the FWeatherFrame value pointer of the first outer this handle addresses. */
	template <typename FrameT>
	FrameT* GetFramePtr(TSharedRef<IPropertyHandle> Handle)
	{
		TArray<void*> Raw;
		Handle->AccessRawData(Raw);
		return (Raw.Num() > 0) ? static_cast<FrameT*>(Raw[0]) : nullptr;
	}

	/** Return the Overrides TMap pointer for a given Frame handle. */
	TMap<FName, FWeatherParamValue>* GetOverridesPtr(TSharedRef<IPropertyHandle> FrameHandle)
	{
		if (TSharedPtr<IPropertyHandle> OH = FrameHandle->GetChildHandle(TEXT("Overrides")))
		{
			TArray<void*> Raw;
			OH->AccessRawData(Raw);
			if (Raw.Num() > 0)
			{
				return static_cast<TMap<FName, FWeatherParamValue>*>(Raw[0]);
			}
		}
		return nullptr;
	}

	/** Resolve the bindings array to use, and (out) the corresponding owning asset. */
	const TArray<FWeatherParamBinding>* ResolveBindings(TSharedRef<IPropertyHandle> FrameHandle, UObject*& OutOwner)
	{
		OutOwner = nullptr;
		TArray<UObject*> Outers;
		FrameHandle->GetOuterObjects(Outers);
		for (UObject* O : Outers)
		{
			if (UWeatherDataAsset* W = Cast<UWeatherDataAsset>(O))
			{
				OutOwner = W;
				return W->Schema ? &W->Schema->Bindings : nullptr;
			}
			if (UWeatherAddonDataAsset* A = Cast<UWeatherAddonDataAsset>(O))
			{
				OutOwner = A;
				return A->Schema ? &A->Schema->Bindings : nullptr;
			}
		}
		return nullptr;
	}

	/** Display order of binding groups: "General" (NAME_None) first, then first-appearance order. */
	TArray<FName> ComputeGroupOrder(const TArray<FWeatherParamBinding>& Bindings)
	{
		TArray<FName> Order;
		for (const FWeatherParamBinding& B : Bindings)
		{
			if (!Order.Contains(B.Group))
			{
				Order.Add(B.Group);
			}
		}
		if (Order.Remove(NAME_None) > 0)
		{
			Order.Insert(NAME_None, 0);
		}
		return Order;
	}

	FText GetGroupDisplayName(FName Group)
	{
		return Group.IsNone() ? NSLOCTEXT("WeatherFrameCustomization", "GeneralGroup", "General") : FText::FromName(Group);
	}
}

// -----------------------------------------------------------------------------------
// CustomizeHeader / Children
// -----------------------------------------------------------------------------------

void FWeatherFrameCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle,
                                                 FDetailWidgetRow& HeaderRow,
                                                 IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructHandle = PropertyHandle;

	HeaderRow
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(200.0f)
	[
		SNew(STextBlock)
		.Text_Lambda([PropertyHandle]() -> FText
		{
			TArray<void*> Raw;
			PropertyHandle->AccessRawData(Raw);
			const FWeatherFrame* Frame = (Raw.Num() > 0) ? static_cast<const FWeatherFrame*>(Raw[0]) : nullptr;
			const float TOD = Frame ? Frame->TimeOfDay : 0.0f;
			return FText::Format(NSLOCTEXT("WeatherFrameCustomization", "FrameHeader", "@ {0}"),
				FText::AsNumber(TOD));
		})
		.Font(IDetailLayoutBuilder::GetDetailFont())
	];
}

UWeatherParamSchema* FWeatherFrameCustomization::ResolveSchema() const
{
	if (!StructHandle.IsValid()) return nullptr;
	TArray<UObject*> Outers;
	StructHandle->GetOuterObjects(Outers);
	for (UObject* O : Outers)
	{
		if (UWeatherDataAsset* W = Cast<UWeatherDataAsset>(O))     return W->Schema;
		if (UWeatherAddonDataAsset* A = Cast<UWeatherAddonDataAsset>(O)) return A->Schema;
	}
	return nullptr;
}

void FWeatherFrameCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle,
                                                   IDetailChildrenBuilder& ChildBuilder,
                                                   IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructHandle    = PropertyHandle;
	OverridesHandle = PropertyHandle->GetChildHandle(TEXT("Overrides"));

	// Keep the property utilities around so add/remove can force a refresh.
	const TSharedRef<IPropertyUtilities> PropUtils = CustomizationUtils.GetPropertyUtilities().ToSharedRef();
	PropertyUtilities = PropUtils;

	// --- 1) TimeOfDay row (default editor) ---
	if (TSharedPtr<IPropertyHandle> TODHandle = PropertyHandle->GetChildHandle(TEXT("TimeOfDay")))
	{
		ChildBuilder.AddProperty(TODHandle.ToSharedRef());
	}

	// --- 2) Overrides section header with "Add" combo ---
	ChildBuilder.AddCustomRow(LOCTEXT("OverridesRow", "Overrides"))
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("OverridesLabel", "Overrides"))
		.Font(IDetailLayoutBuilder::GetDetailFontBold())
	]
	.ValueContent()
	.MinDesiredWidth(200.0f)
	[
		SNew(SComboButton)
		.ContentPadding(FMargin(6.0f, 2.0f))
		.OnGetMenuContent_Lambda([this, PropUtils]() -> TSharedRef<SWidget>
		{
			return BuildAddMenu();
		})
		.ButtonContent()
		[
			SNew(STextBlock).Text(LOCTEXT("AddOverride", "+ Add Override"))
		]
	];

	// --- 3) One row per existing override, in schema order ---
	UObject* Owner = nullptr;
	const TArray<FWeatherParamBinding>* Bindings = ResolveBindings(PropertyHandle, Owner);
	TMap<FName, FWeatherParamValue>* Map = GetOverridesPtr(PropertyHandle);
	if (!Bindings || !Map || !Owner)
	{
		ChildBuilder.AddCustomRow(LOCTEXT("NoSchemaRow", "No Schema"))
		.WholeRowContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoSchema", "Set a Schema on the owning asset to edit overrides."))
			.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.6f, 0.3f)))
		];
		return;
	}

	// Emit one override row per binding (shared body for the grouped loop below).
	auto AddOverrideRow = [this, Map, Owner, PropUtils, &ChildBuilder](const FWeatherParamBinding& B)
	{
		FWeatherParamValue* V = Map->Find(B.ParamName);
		if (!V) return;

		const FText Label = B.DisplayName.IsEmpty()
			? FText::FromName(B.ParamName)
			: B.DisplayName;

		FDetailWidgetRow& Row = ChildBuilder.AddCustomRow(Label);
		Row.NameContent()
		[
			SNew(STextBlock).Text(Label).Font(IDetailLayoutBuilder::GetDetailFont())
		];

		TSharedRef<SHorizontalBox> ValueBox = SNew(SHorizontalBox);
		const FName Key = B.ParamName;

		// Value editor by type
		switch (B.Type)
		{
		case EWeatherParamType::Float:
		{
			// Slider range is informational (Binding->FloatMin/FloatMax); input isn't clamped.
			const bool bValidRange = B.FloatMax >= B.FloatMin;
			ValueBox->AddSlot().FillWidth(1.0f)
			[
				SNew(SSpinBox<float>)
				.MinValue(TOptional<float>())
				.MaxValue(TOptional<float>())
				.MinSliderValue(bValidRange ? TOptional<float>(B.FloatMin) : TOptional<float>())
				.MaxSliderValue(bValidRange ? TOptional<float>(B.FloatMax) : TOptional<float>())
				.Value_Lambda([Map, Key]() -> float
				{
					const FWeatherParamValue* Vp = Map->Find(Key);
					return Vp ? Vp->FloatValue : 0.0f;
				})
				.OnValueChanged_Lambda([Map, Key, Owner](float NewVal)
				{
					if (FWeatherParamValue* Vp = Map->Find(Key))
					{
						Vp->FloatValue = NewVal;
						if (Owner) Owner->MarkPackageDirty();
					}
				})
				.OnValueCommitted_Lambda([Map, Key, Owner, PropUtils](float NewVal, ETextCommit::Type)
				{
					if (FWeatherParamValue* Vp = Map->Find(Key))
					{
						Vp->FloatValue = NewVal;
						if (Owner)
						{
							Owner->Modify();
							Owner->MarkPackageDirty();
						}
					}
				})
			];
			break;
		}
		case EWeatherParamType::LinearColor:
		{
			ValueBox->AddSlot().FillWidth(1.0f)
			[
				SNew(SColorBlock)
				.Color_Lambda([Map, Key]() -> FLinearColor
				{
					const FWeatherParamValue* Vp = Map->Find(Key);
					return Vp ? Vp->LinearColorValue : FLinearColor::White;
				})
				.ShowBackgroundForAlpha(true)
				.OnMouseButtonDown_Lambda([Map, Key, Owner](const FGeometry&, const FPointerEvent& E) -> FReply
				{
					if (E.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
					FColorPickerArgs Args;
					Args.bUseAlpha = true;
					Args.bOnlyRefreshOnMouseUp = false;
					Args.InitialColor = Map->Contains(Key) ? (*Map)[Key].LinearColorValue : FLinearColor::White;
					Args.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([Map, Key, Owner](FLinearColor C)
					{
						if (FWeatherParamValue* Vp = Map->Find(Key))
						{
							Vp->LinearColorValue = C;
							if (Owner) { Owner->Modify(); Owner->MarkPackageDirty(); }
						}
					});
					OpenColorPicker(Args);
					return FReply::Handled();
				})
			];
			break;
		}
		case EWeatherParamType::Color:
		{
			ValueBox->AddSlot().FillWidth(1.0f)
			[
				SNew(SColorBlock)
				.Color_Lambda([Map, Key]() -> FLinearColor
				{
					const FWeatherParamValue* Vp = Map->Find(Key);
					return Vp ? FLinearColor(Vp->ColorValue) : FLinearColor::White;
				})
				.ShowBackgroundForAlpha(true)
				.OnMouseButtonDown_Lambda([Map, Key, Owner](const FGeometry&, const FPointerEvent& E) -> FReply
				{
					if (E.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
					FColorPickerArgs Args;
					Args.bUseAlpha = true;
					Args.bOnlyRefreshOnMouseUp = false;
					Args.InitialColor = Map->Contains(Key) ? FLinearColor((*Map)[Key].ColorValue) : FLinearColor::White;
					Args.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([Map, Key, Owner](FLinearColor C)
					{
						if (FWeatherParamValue* Vp = Map->Find(Key))
						{
							Vp->ColorValue = C.ToFColor(true);
							if (Owner) { Owner->Modify(); Owner->MarkPackageDirty(); }
						}
					});
					OpenColorPicker(Args);
					return FReply::Handled();
				})
			];
			break;
		}
		}

		// Delete button
		ValueBox->AddSlot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ToolTipText(LOCTEXT("RemoveOverride", "Remove this override"))
			.OnClicked_Lambda([this, Key, PropUtils]() -> FReply
			{
				RemoveOverride(Key);
				PropUtils->ForceRefresh();
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("×")))
			]
		];

		Row.ValueContent()
		.MinDesiredWidth(220.0f)
		[
			ValueBox
		];
	};

	// Group the override rows by binding Group (General first, then first-appearance order).
	const TArray<FName> GroupOrder = ComputeGroupOrder(*Bindings);
	for (const FName Group : GroupOrder)
	{
		// Only show a group header when at least one of its bindings is overridden.
		bool bHasAny = false;
		for (const FWeatherParamBinding& B : *Bindings)
		{
			if (B.Group == Group && Map->Find(B.ParamName))
			{
				bHasAny = true;
				break;
			}
		}
		if (!bHasAny)
		{
			continue;
		}

		ChildBuilder.AddCustomRow(GetGroupDisplayName(Group))
		.WholeRowContent()
		[
			SNew(STextBlock)
			.Text(GetGroupDisplayName(Group))
			.Font(IDetailLayoutBuilder::GetDetailFontBold())
		];

		for (const FWeatherParamBinding& B : *Bindings)
		{
			if (B.Group != Group)
			{
				continue;
			}
			AddOverrideRow(B);
		}
	}
}

// -----------------------------------------------------------------------------------
// Add / Remove
// -----------------------------------------------------------------------------------

TSharedRef<SWidget> FWeatherFrameCustomization::BuildAddMenu()
{
	UObject* Owner = nullptr;
	const TArray<FWeatherParamBinding>* Bindings = StructHandle.IsValid()
		? ResolveBindings(StructHandle.ToSharedRef(), Owner)
		: nullptr;
	TMap<FName, FWeatherParamValue>* Map = StructHandle.IsValid()
		? GetOverridesPtr(StructHandle.ToSharedRef())
		: nullptr;

	TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
	if (!Bindings || !Map)
	{
		Menu->AddSlot().AutoHeight().Padding(6.0f)
		[
			SNew(STextBlock).Text(LOCTEXT("NoBindings", "No schema bindings available."))
		];
		return Menu;
	}

	bool bAny = false;
	const TArray<FName> GroupOrder = ComputeGroupOrder(*Bindings);
	for (const FName Group : GroupOrder)
	{
		// Collect this group's entries that are not yet overridden.
		TArray<const FWeatherParamBinding*> GroupEntries;
		for (const FWeatherParamBinding& B : *Bindings)
		{
			if (B.Group != Group) continue;
			if (Map->Contains(B.ParamName)) continue;
			GroupEntries.Add(&B);
		}
		if (GroupEntries.Num() == 0) continue;
		bAny = true;

		Menu->AddSlot().AutoHeight().Padding(6.0f, 8.0f, 6.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(GetGroupDisplayName(Group))
			.Font(IDetailLayoutBuilder::GetDetailFontBold())
			.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)))
		];

		for (const FWeatherParamBinding* Bp : GroupEntries)
		{
			const FText Label = Bp->DisplayName.IsEmpty() ? FText::FromName(Bp->ParamName) : Bp->DisplayName;
			const FName Key = Bp->ParamName;
			Menu->AddSlot().AutoHeight()
			[
				SNew(SButton)
				.HAlign(HAlign_Left)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ContentPadding(FMargin(8.0f, 3.0f))
				.OnClicked_Lambda([this, Key]() -> FReply
				{
					AddOverride(Key);
					FSlateApplication::Get().DismissAllMenus();
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Text(Label)
				]
			];
		}
	}
	if (!bAny)
	{
		Menu->AddSlot().AutoHeight().Padding(6.0f)
		[
			SNew(STextBlock).Text(LOCTEXT("AllBound", "All schema entries are already overridden."))
		];
	}
	return Menu;
}

void FWeatherFrameCustomization::AddOverride(FName ParamName)
{
	if (!StructHandle.IsValid()) return;
	UObject* Owner = nullptr;
	const TArray<FWeatherParamBinding>* Bindings = ResolveBindings(StructHandle.ToSharedRef(), Owner);
	TMap<FName, FWeatherParamValue>* Map = GetOverridesPtr(StructHandle.ToSharedRef());
	if (!Bindings || !Map || !Owner) return;

	const FWeatherParamBinding* B = Bindings->FindByPredicate([&](const FWeatherParamBinding& X){ return X.ParamName == ParamName; });
	if (!B) return;

	const FScopedTransaction Tx(LOCTEXT("AddOverrideTx", "Add Weather Override"));
	Owner->Modify();
	Map->Add(ParamName, B->MakeDefaultValue());
	Owner->MarkPackageDirty();
	StructHandle->NotifyPostChange(EPropertyChangeType::ArrayAdd);

	if (TSharedPtr<IPropertyUtilities> Utils = PropertyUtilities.Pin())
	{
		Utils->ForceRefresh();
	}
}

void FWeatherFrameCustomization::RemoveOverride(FName ParamName)
{
	if (!StructHandle.IsValid()) return;
	UObject* Owner = nullptr;
	ResolveBindings(StructHandle.ToSharedRef(), Owner);
	TMap<FName, FWeatherParamValue>* Map = GetOverridesPtr(StructHandle.ToSharedRef());
	if (!Map || !Owner) return;

	const FScopedTransaction Tx(LOCTEXT("RemoveOverrideTx", "Remove Weather Override"));
	Owner->Modify();
	Map->Remove(ParamName);
	Owner->MarkPackageDirty();
	StructHandle->NotifyPostChange(EPropertyChangeType::ArrayRemove);
}

#undef LOCTEXT_NAMESPACE
