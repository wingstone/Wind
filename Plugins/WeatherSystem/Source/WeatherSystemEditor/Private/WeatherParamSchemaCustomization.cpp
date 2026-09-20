// Copyright TADemo. All Rights Reserved.

#include "WeatherParamSchemaCustomization.h"

#include "Components/LightComponent.h"
#include "Components/MeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Data/WeatherParamSchema.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "IPropertyUtilities.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WeatherParamSchemaCustomization"

TSharedRef<IDetailCustomization> FWeatherParamSchemaCustomization::MakeInstance()
{
	return MakeShared<FWeatherParamSchemaCustomization>();
}

namespace
{
	FText GetSchemaGroupDisplayName(FName Group)
	{
		return Group.IsNone() ? LOCTEXT("GeneralGroup", "General") : FText::FromName(Group);
	}

	/** Enumerate the element handles of an array property handle. */
	TArray<TSharedRef<IPropertyHandle>> GetArrayElements(const TSharedPtr<IPropertyHandleArray>& Arr)
	{
		TArray<TSharedRef<IPropertyHandle>> Elements;
		if (!Arr.IsValid())
		{
			return Elements;
		}
		uint32 Num = 0;
		if (Arr->GetNumElements(Num) != FPropertyAccess::Success)
		{
			return Elements;
		}
		Elements.Reserve(Num);
		for (uint32 i = 0; i < Num; ++i)
		{
			Elements.Add(Arr->GetElement(i));
		}
		return Elements;
	}

	/** Current editor world (falls back to the PIE world when running in PIE). */
	UWorld* GetEditorWorld()
	{
		if (!GEditor)
		{
			return nullptr;
		}
		if (FWorldContext* PIECtx = GEditor->GetPIEWorldContext())
		{
			return PIECtx->World();
		}
		return GEditor->GetEditorWorldContext(false).World();
	}

	/** Scalar + vector parameter names exposed by a material interface. */
	void GetMaterialParameters(UMaterialInterface* Mat, TArray<FName>& OutScalars, TArray<FName>& OutVectors)
	{
		OutScalars.Reset();
		OutVectors.Reset();
		if (!Mat)
		{
			return;
		}
		TArray<FMaterialParameterInfo> Info;
		TArray<FGuid> Ids;
		Mat->GetAllScalarParameterInfo(Info, Ids);
		for (const FMaterialParameterInfo& P : Info)
		{
			OutScalars.Add(P.Name);
		}
		Info.Reset();
		Ids.Reset();
		Mat->GetAllVectorParameterInfo(Info, Ids);
		for (const FMaterialParameterInfo& P : Info)
		{
			OutVectors.Add(P.Name);
		}
	}

	/** The ComponentTag the applier should use to find this component: first non-empty tag, else its object name. */
	FName GetComponentTag(UActorComponent* Component)
	{
		if (Component)
		{
			for (const FName& Tag : Component->ComponentTags)
			{
				if (!Tag.IsNone())
				{
					return Tag;
				}
			}
			return Component->GetFName();
		}
		return NAME_None;
	}

	/** Add one leaf menu entry that, when clicked, calls OnPick with a ready-to-use binding template. */
	void AddBindingEntry(FMenuBuilder& MenuBuilder, const FText& Label, const FWeatherParamBinding& Template,
	                     const TFunction<void(const FWeatherParamBinding&)>& OnPick)
	{
		MenuBuilder.AddMenuEntry(
			Label, FText::GetEmpty(), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([OnPick, Template]()
			{
				OnPick(Template);
			})));
	}

	/** Material -> "Scalar / Vector" parameter submenu (mesh material or light function). */
	void AddMaterialSubmenu(FMenuBuilder& MenuBuilder, const FText& Label, UMaterialInterface* Mat,
	                        FName Group, FName ComponentTag,
	                        const TFunction<void(const FWeatherParamBinding&)>& OnPick)
	{
		TArray<FName> Scalars, Vectors;
		GetMaterialParameters(Mat, Scalars, Vectors);
		if (Scalars.Num() == 0 && Vectors.Num() == 0)
		{
			return;
		}

		MenuBuilder.AddSubMenu(Label, FText::GetEmpty(),
			FNewMenuDelegate::CreateLambda([Scalars, Vectors, Group, ComponentTag, OnPick](FMenuBuilder& Sub)
			{
				if (Scalars.Num() > 0)
				{
					Sub.BeginSection("Scalar", LOCTEXT("ScalarParams", "Scalar"));
					for (const FName& N : Scalars)
					{
						FWeatherParamBinding Tpl;
						Tpl.Target = EWeatherParamTarget::MaterialParameter;
						Tpl.Type = EWeatherParamType::Float;
						Tpl.ParamName = N;
						Tpl.DisplayName = FText::FromName(N);
						Tpl.ComponentTag = ComponentTag;
						AddBindingEntry(Sub, FText::FromName(N), Tpl, OnPick);
					}
					Sub.EndSection();
				}
				if (Vectors.Num() > 0)
				{
					Sub.BeginSection("Vector", LOCTEXT("VectorParams", "Vector"));
					for (const FName& N : Vectors)
					{
						FWeatherParamBinding Tpl;
						Tpl.Target = EWeatherParamTarget::MaterialParameter;
						Tpl.Type = EWeatherParamType::LinearColor;
						Tpl.ParamName = N;
						Tpl.DisplayName = FText::FromName(N);
						Tpl.ComponentTag = ComponentTag;
						AddBindingEntry(Sub, FText::FromName(N), Tpl, OnPick);
					}
					Sub.EndSection();
				}
			}));
	}

	/** Component -> editable scalar / color UPROPERTYs (ActorProperty bindings). */
	void AddPropertySubmenu(FMenuBuilder& MenuBuilder, const FText& Label, UObject* Owner,
	                        FName Group, FName ComponentTag,
	                        const TFunction<void(const FWeatherParamBinding&)>& OnPick)
	{
		struct FPropEntry
		{
			FName PropName;
			EWeatherParamType Type;
			UFunction* Setter; // null when no matching setter was found on the class
		};
		TArray<FPropEntry> Props;

		UClass* OwnerClass = Owner->GetClass();

		// Locate a Set<PropName>(<type>) UFunction on the owner's class whose single non-return
		// parameter matches the property's type. Returns null when no clean match exists — the
		// caller will fall back to the raw UPROPERTY path.
		auto FindMatchingSetter = [](UClass* Cls, FName PropName, EWeatherParamType Type) -> UFunction*
		{
			if (!Cls)
			{
				return nullptr;
			}
			const FName SetterName(*(FString(TEXT("Set")) + PropName.ToString()));
			UFunction* F = Cls->FindFunctionByName(SetterName);
			if (!F)
			{
				return nullptr;
			}
			// Must be reflection-callable at runtime by our applier (ProcessEvent path).
			if (!F->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_Native))
			{
				return nullptr;
			}
			// Exactly one non-return input parameter.
			FProperty* SingleParam = nullptr;
			for (TFieldIterator<FProperty> It(F); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				FProperty* Param = *It;
				if (Param->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					continue;
				}
				if (Param->HasAnyPropertyFlags(CPF_OutParm))
				{
					return nullptr; // out/ref parameters can't be driven from a single value
				}
				if (SingleParam)
				{
					return nullptr; // more than one input parameter
				}
				SingleParam = Param;
			}
			if (!SingleParam)
			{
				return nullptr;
			}
			// Type must line up with what the binding will send.
			switch (Type)
			{
			case EWeatherParamType::Float:
				if (!CastField<FFloatProperty>(SingleParam) && !CastField<FDoubleProperty>(SingleParam))
				{
					return nullptr;
				}
				break;
			case EWeatherParamType::LinearColor:
				if (FStructProperty* Sp = CastField<FStructProperty>(SingleParam))
				{
					if (Sp->Struct != TBaseStructure<FLinearColor>::Get())
					{
						return nullptr;
					}
				}
				else
				{
					return nullptr;
				}
				break;
			case EWeatherParamType::Color:
				if (FStructProperty* Sp = CastField<FStructProperty>(SingleParam))
				{
					// Applier promotes Color -> LinearColor when the setter takes LinearColor, so either is fine.
					if (Sp->Struct != TBaseStructure<FColor>::Get() && Sp->Struct != TBaseStructure<FLinearColor>::Get())
					{
						return nullptr;
					}
				}
				else
				{
					return nullptr;
				}
				break;
			default:
				return nullptr;
			}
			return F;
		};

		for (TFieldIterator<FProperty> It(OwnerClass); It; ++It)
		{
			FProperty* P = *It;
			if (!P->HasAnyPropertyFlags(CPF_Edit))
			{
				continue;
			}
			EWeatherParamType Type;
			if (CastField<FFloatProperty>(P) || CastField<FDoubleProperty>(P))
			{
				Type = EWeatherParamType::Float;
			}
			else if (FStructProperty* Sp = CastField<FStructProperty>(P))
			{
				if (Sp->Struct == TBaseStructure<FLinearColor>::Get())
				{
					Type = EWeatherParamType::LinearColor;
				}
				else if (Sp->Struct == TBaseStructure<FColor>::Get())
				{
					Type = EWeatherParamType::Color;
				}
				else
				{
					continue;
				}
			}
			else
			{
				continue;
			}

			UFunction* Setter = FindMatchingSetter(OwnerClass, P->GetFName(), Type);
			Props.Add({ P->GetFName(), Type, Setter });
		}
		if (Props.Num() == 0)
		{
			return;
		}

		MenuBuilder.AddSubMenu(Label, FText::GetEmpty(),
			FNewMenuDelegate::CreateLambda([Props, Group, ComponentTag, OnPick](FMenuBuilder& Sub)
			{
				for (const FPropEntry& P : Props)
				{
					FWeatherParamBinding Tpl;
					Tpl.Type = P.Type;
					Tpl.ComponentTag = ComponentTag;
					Tpl.DisplayName = FText::FromName(P.PropName);

					FText EntryLabel;
					FText EntryTip;
					if (P.Setter)
					{
						// Prefer the setter — running through the UFunction fires
						// MarkRenderStateDirty / property-change side effects the raw
						// UPROPERTY write skips.
						Tpl.Target = EWeatherParamTarget::ActorFunction;
						Tpl.ParamName = P.Setter->GetFName();
						EntryLabel = FText::Format(LOCTEXT("PropWithSetterFmt", "{0}  (setter)"), FText::FromName(P.PropName));
						EntryTip = FText::Format(LOCTEXT("PropWithSetterTip", "Calls {0}() so the component refreshes properly."),
							FText::FromName(P.Setter->GetFName()));
					}
					else
					{
						Tpl.Target = EWeatherParamTarget::ActorProperty;
						Tpl.ParamName = P.PropName;
						EntryLabel = FText::FromName(P.PropName);
						EntryTip = FText::Format(LOCTEXT("PropRawTip", "Writes UPROPERTY '{0}' directly."), FText::FromName(P.PropName));
					}

					Sub.AddMenuEntry(
						EntryLabel, EntryTip, FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([OnPick, Tpl]()
						{
							OnPick(Tpl);
						})));
				}
			}));
	}
}

void FWeatherParamSchemaCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	const TSharedRef<IPropertyUtilities> Utils = DetailBuilder.GetPropertyUtilities();
	PropertyUtilities = Utils;

	// Remember the edited schema so the add-binding menu can read its MPC.
	Schema = nullptr;
	{
		TArray<TWeakObjectPtr<UObject>> CustomizedObjects;
		DetailBuilder.GetObjectsBeingCustomized(CustomizedObjects);
		for (const TWeakObjectPtr<UObject>& Obj : CustomizedObjects)
		{
			if (UWeatherParamSchema* S = Cast<UWeatherParamSchema>(Obj.Get()))
			{
				Schema = S;
				break;
			}
		}
	}

	IDetailCategoryBuilder& Cat = DetailBuilder.EditCategory("Weather");

	// Re-add MPC at the top for deterministic ordering before the grouped bindings.
	if (TSharedPtr<IPropertyHandle> MPCHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UWeatherParamSchema, MPC)))
	{
		DetailBuilder.HideProperty(MPCHandle);
		Cat.AddProperty(MPCHandle);
	}

	BindingsHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UWeatherParamSchema, Bindings));
	DetailBuilder.HideProperty(BindingsHandle);
	if (!BindingsHandle.IsValid() || !BindingsHandle->AsArray().IsValid())
	{
		return;
	}
	const TSharedPtr<IPropertyHandleArray> Arr = BindingsHandle->AsArray();

	// The built-in trash-can button on an array element row removes the element from
	// the underlying array, but our customization built its per-group index map from a
	// snapshot of the array — the leftover IPropertyHandles keep rendering as empty
	// rows until we rebuild the layout. Force a refresh when the element count changes.
	Arr->SetOnNumElementsChanged(FSimpleDelegate::CreateLambda([Utils]()
	{
		Utils->ForceRefresh();
	}));

	// Bucket element handles by Group, preserving first-appearance order; "General" (NAME_None) first.
	const TArray<TSharedRef<IPropertyHandle>> Elements = GetArrayElements(Arr);

	TArray<FName> GroupOrder;
	TMap<FName, TArray<int32>> GroupToIndices;
	if (Elements.Num() == 0)
	{
		GroupOrder.Add(NAME_None); // always show at least the General section so bindings can be added
	}

	for (int32 i = 0; i < Elements.Num(); ++i)
	{
		FName Group = NAME_None;
		if (TSharedPtr<IPropertyHandle> GroupHandle = Elements[i]->GetChildHandle(GET_MEMBER_NAME_CHECKED(FWeatherParamBinding, Group)))
		{
			GroupHandle->GetValue(Group);
		}
		if (!GroupToIndices.Contains(Group))
		{
			GroupOrder.Add(Group);
		}
		GroupToIndices.FindOrAdd(Group).Add(i);
	}
	if (GroupToIndices.Contains(NAME_None))
	{
		GroupOrder.Remove(NAME_None);
		GroupOrder.Insert(NAME_None, 0);
	}

	for (const FName Group : GroupOrder)
	{
		const TArray<int32>* Indices = GroupToIndices.Find(Group);
		const FText GroupLabel = GetSchemaGroupDisplayName(Group);

		// Group header: name, count, and "+ Add Binding" button.
		Cat.AddCustomRow(GroupLabel)
		.NameContent()
		[
			SNew(STextBlock)
			.Text(GroupLabel)
			.Font(IDetailLayoutBuilder::GetDetailFontBold())
		]
		.ValueContent()
		.MinDesiredWidth(160.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("BindingCount", "({0})"), FText::AsNumber(Indices ? Indices->Num() : 0)))
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SComboButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(FText::Format(LOCTEXT("AddBindingTip", "Add a binding to \"{0}\". Pick a target (actor / MPC) to auto-fill it, or add a blank one."), GroupLabel))
				.OnGetMenuContent_Lambda([this, Group]() -> TSharedRef<SWidget>
				{
					return BuildAddBindingMenuWidget(Group);
				})
				.HasDownArrow(false)
				.ContentPadding(FMargin(4.0f, 2.0f))
				.ButtonContent()
				[
					SNew(STextBlock).Text(LOCTEXT("AddBinding", "+ Add Binding"))
				]
			]
		];

		if (!Indices)
		{
			continue;
		}

		for (const int32 Idx : *Indices)
		{
			TSharedRef<IPropertyHandle> Elem = Elements[Idx];
			Cat.AddProperty(Elem);

			// Re-sort the view when the binding's Group field changes.
			if (TSharedPtr<IPropertyHandle> GroupHandle = Elem->GetChildHandle(GET_MEMBER_NAME_CHECKED(FWeatherParamBinding, Group)))
			{
				GroupHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([Utils]()
				{
					Utils->RequestRefresh();
				}));
			}
		}
	}
}

int32 FWeatherParamSchemaCustomization::AddBindingWithTemplate(FName GroupName, const FWeatherParamBinding& Template)
{
	if (!BindingsHandle.IsValid())
	{
		return INDEX_NONE;
	}
	const TSharedPtr<IPropertyHandleArray> Arr = BindingsHandle->AsArray();
	if (!Arr.IsValid())
	{
		return INDEX_NONE;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddBindingTx", "Add Weather Binding"));

	// Insert right after this group's last element so the new binding appears inside its group.
	int32 InsertIndex = INDEX_NONE;
	{
		const TArray<TSharedRef<IPropertyHandle>> Elements = GetArrayElements(Arr);
		for (int32 i = Elements.Num() - 1; i >= 0; --i)
		{
			FName G = NAME_None;
			if (TSharedPtr<IPropertyHandle> GroupHandle = Elements[i]->GetChildHandle(GET_MEMBER_NAME_CHECKED(FWeatherParamBinding, Group)))
			{
				GroupHandle->GetValue(G);
			}
			if (G == GroupName)
			{
				InsertIndex = i + 1;
				break;
			}
		}
	}

	int32 NewIndex = INDEX_NONE;
	uint32 Num = 0;
	Arr->GetNumElements(Num);
	if (InsertIndex == INDEX_NONE || InsertIndex >= (int32)Num)
	{
		// Append at the end (empty group, or the group's last element is already last).
		const FPropertyHandleItemAddResult Res = Arr->AddItem();
		if (Res.GetAccessResult() == FPropertyAccess::Success)
		{
			NewIndex = Res.GetIndex();
		}
	}
	else
	{
		if (Arr->Insert(InsertIndex) == FPropertyAccess::Success)
		{
			NewIndex = InsertIndex;
		}
	}

	if (NewIndex != INDEX_NONE)
	{
		TSharedRef<IPropertyHandle> NewElem = Arr->GetElement(NewIndex);

		// Auto-fill the identity fields from the template; defaults stay at struct defaults.
		if (TSharedPtr<IPropertyHandle> P = NewElem->GetChildHandle(GET_MEMBER_NAME_CHECKED(FWeatherParamBinding, ParamName)))   { P->SetValue(Template.ParamName); }
		if (TSharedPtr<IPropertyHandle> P = NewElem->GetChildHandle(GET_MEMBER_NAME_CHECKED(FWeatherParamBinding, DisplayName))) { P->SetValue(Template.DisplayName); }
		if (TSharedPtr<IPropertyHandle> P = NewElem->GetChildHandle(GET_MEMBER_NAME_CHECKED(FWeatherParamBinding, Target)))       { P->SetValue((uint8)Template.Target); }
		if (TSharedPtr<IPropertyHandle> P = NewElem->GetChildHandle(GET_MEMBER_NAME_CHECKED(FWeatherParamBinding, Type)))         { P->SetValue((uint8)Template.Type); }
		if (TSharedPtr<IPropertyHandle> P = NewElem->GetChildHandle(GET_MEMBER_NAME_CHECKED(FWeatherParamBinding, ComponentTag))) { P->SetValue(Template.ComponentTag); }
		if (TSharedPtr<IPropertyHandle> P = NewElem->GetChildHandle(GET_MEMBER_NAME_CHECKED(FWeatherParamBinding, Group)))        { P->SetValue(GroupName); }
	}

	if (TSharedPtr<IPropertyUtilities> Utils = PropertyUtilities.Pin())
	{
		Utils->ForceRefresh();
	}
	return NewIndex;
}

void FWeatherParamSchemaCustomization::AddBlankBinding(FName GroupName)
{
	FWeatherParamBinding Tpl;
	Tpl.ParamName = GenerateUniqueParamName();
	Tpl.DisplayName = FText::FromName(Tpl.ParamName);
	AddBindingWithTemplate(GroupName, Tpl);
}

FName FWeatherParamSchemaCustomization::GenerateUniqueParamName() const
{
	TSet<FName> Existing;
	if (BindingsHandle.IsValid())
	{
		const TArray<TSharedRef<IPropertyHandle>> Elements = GetArrayElements(BindingsHandle->AsArray());
		for (const TSharedRef<IPropertyHandle>& E : Elements)
		{
			FName N;
			if (TSharedPtr<IPropertyHandle> P = E->GetChildHandle(GET_MEMBER_NAME_CHECKED(FWeatherParamBinding, ParamName)))
			{
				P->GetValue(N);
			}
			Existing.Add(N);
		}
	}
	if (!Existing.Contains(FName(TEXT("Param"))))
	{
		return FName(TEXT("Param"));
	}
	int32 Counter = 1;
	while (Existing.Contains(FName(FString::Printf(TEXT("Param_%d"), Counter))))
	{
		++Counter;
	}
	return FName(FString::Printf(TEXT("Param_%d"), Counter));
}

// -----------------------------------------------------------------------------------
// Add-binding menu
// -----------------------------------------------------------------------------------

TSharedRef<SWidget> FWeatherParamSchemaCustomization::BuildAddBindingMenuWidget(FName Group)
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.BeginSection("Target", LOCTEXT("FromTarget", "From Target"));
	{
		// MPC parameters (schema's single MPC).
		if (Schema.IsValid() && Schema->MPC.IsValid())
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("MPCParams", "MPC Parameters"),
				LOCTEXT("MPCParamsTip", "Add a binding that writes into the schema's material parameter collection."),
				FNewMenuDelegate::CreateSP(this, &FWeatherParamSchemaCustomization::BuildMPCMenu, Group));
		}

		// Level actor -> component -> material parameter / property.
		MenuBuilder.AddSubMenu(
			LOCTEXT("FromActor", "From Actor..."),
			LOCTEXT("FromActorTip", "Pick a level actor, then a component, then a material parameter or a component property."),
			FNewMenuDelegate::CreateSP(this, &FWeatherParamSchemaCustomization::BuildActorsMenu, Group));
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Misc");
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("BlankBinding", "Blank Binding"),
			LOCTEXT("BlankBindingTip", "Add an empty binding with an auto-generated name to this group."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FWeatherParamSchemaCustomization::AddBlankBinding, Group)));
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void FWeatherParamSchemaCustomization::BuildMPCMenu(FMenuBuilder& MenuBuilder, FName Group)
{
	UMaterialParameterCollection* MPC = Schema.IsValid() ? Schema->GetMPC() : nullptr;
	if (!MPC)
	{
		MenuBuilder.AddMenuEntry(LOCTEXT("NoMPC", "No MPC set on this schema"), FText::GetEmpty(), FSlateIcon(), FUIAction());
		return;
	}

	const auto OnPick = [this, Group](const FWeatherParamBinding& Tpl)
	{
		AddBindingWithTemplate(Group, Tpl);
	};

	MenuBuilder.BeginSection("Scalar", LOCTEXT("ScalarParams", "Scalar"));
	for (const FCollectionScalarParameter& P : MPC->ScalarParameters)
	{
		FWeatherParamBinding Tpl;
		Tpl.Target = EWeatherParamTarget::MPCParameter;
		Tpl.Type = EWeatherParamType::Float;
		Tpl.ParamName = P.ParameterName;
		Tpl.DisplayName = FText::FromName(P.ParameterName);
		AddBindingEntry(MenuBuilder, FText::FromName(P.ParameterName), Tpl, OnPick);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Vector", LOCTEXT("VectorParams", "Vector"));
	for (const FCollectionVectorParameter& P : MPC->VectorParameters)
	{
		FWeatherParamBinding Tpl;
		Tpl.Target = EWeatherParamTarget::MPCParameter;
		Tpl.Type = EWeatherParamType::LinearColor;
		Tpl.ParamName = P.ParameterName;
		Tpl.DisplayName = FText::FromName(P.ParameterName);
		AddBindingEntry(MenuBuilder, FText::FromName(P.ParameterName), Tpl, OnPick);
	}
	MenuBuilder.EndSection();
}

void FWeatherParamSchemaCustomization::BuildActorsMenu(FMenuBuilder& MenuBuilder, FName Group)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		MenuBuilder.AddMenuEntry(LOCTEXT("NoWorld", "No editor world available"), FText::GetEmpty(), FSlateIcon(), FUIAction());
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}
		FString Label = Actor->GetActorLabel();
		if (Label.IsEmpty())
		{
			Label = Actor->GetName();
		}
		MenuBuilder.AddSubMenu(
			FText::FromString(Label),
			FText::FromString(Actor->GetClass()->GetName()),
			FNewMenuDelegate::CreateSP(this, &FWeatherParamSchemaCustomization::BuildActorComponentsMenu, Group, TWeakObjectPtr<AActor>(Actor)));
	}
}

void FWeatherParamSchemaCustomization::BuildActorComponentsMenu(FMenuBuilder& MenuBuilder, FName Group, TWeakObjectPtr<AActor> ActorPtr)
{
	AActor* Actor = ActorPtr.Get();
	if (!Actor)
	{
		return;
	}
	for (UActorComponent* Comp : Actor->GetComponents())
	{
		if (!Comp)
		{
			continue;
		}
		MenuBuilder.AddSubMenu(
			FText::FromString(Comp->GetName()),
			FText::FromString(Comp->GetClass()->GetName()),
			FNewMenuDelegate::CreateSP(this, &FWeatherParamSchemaCustomization::BuildComponentMenu, Group, Comp));
	}
}

void FWeatherParamSchemaCustomization::BuildComponentMenu(FMenuBuilder& MenuBuilder, FName Group, UActorComponent* Component)
{
	if (!Component)
	{
		return;
	}

	const FName ComponentTag = GetComponentTag(Component);
	const auto OnPick = [this, Group](const FWeatherParamBinding& Tpl)
	{
		AddBindingWithTemplate(Group, Tpl);
	};

	// Mesh material (element 0) parameters.
	if (UMeshComponent* Mesh = Cast<UMeshComponent>(Component))
	{
		if (UMaterialInterface* Mat = Mesh->GetMaterial(0))
		{
			AddMaterialSubmenu(MenuBuilder, LOCTEXT("MaterialParams", "Material Parameters"), Mat, Group, ComponentTag, OnPick);
		}
	}

	// Light function material parameters.
	if (ULightComponent* Light = Cast<ULightComponent>(Component))
	{
		if (Light->LightFunctionMaterial)
		{
			AddMaterialSubmenu(MenuBuilder, LOCTEXT("LightFunctionParams", "Light Function Parameters"), Light->LightFunctionMaterial, Group, ComponentTag, OnPick);
		}
	}

	// Volumetric cloud material parameters.
	if (UVolumetricCloudComponent* Cloud = Cast<UVolumetricCloudComponent>(Component))
	{
		if (UMaterialInterface* CloudMat = Cloud->Material.LoadSynchronous())
		{
			AddMaterialSubmenu(MenuBuilder, LOCTEXT("CloudMaterialParams", "Cloud Material Parameters"), CloudMat, Group, ComponentTag, OnPick);
		}
	}

	// Editable float / color UPROPERTYs on the component.
	AddPropertySubmenu(MenuBuilder, LOCTEXT("ComponentProperties", "Properties"), Component, Group, ComponentTag, OnPick);
}

#undef LOCTEXT_NAMESPACE
