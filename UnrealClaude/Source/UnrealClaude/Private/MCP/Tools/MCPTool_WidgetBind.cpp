// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_WidgetBind.h"
#include "UnrealClaudeModule.h"
#include "MCP/MCPParamValidator.h"

#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/Slider.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/RichTextBlock.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/GridPanel.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/Spacer.h"
#include "Components/Throbber.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"

#include "Kismet2/KismetEditorUtilities.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Layout/Margin.h"

FMCPToolResult FMCPTool_WidgetBind::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	if (Operation == TEXT("create"))
	{
		return ExecuteCreate(Params);
	}
	if (Operation == TEXT("add_widget"))
	{
		return ExecuteAddWidget(Params);
	}
	if (Operation == TEXT("query"))
	{
		return ExecuteQuery(Params);
	}
	if (Operation == TEXT("inspect_slot"))
	{
		return ExecuteInspectSlot(Params);
	}
	if (Operation == TEXT("set_slot"))
	{
		return ExecuteSetSlot(Params);
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: %s. Valid: create, add_widget, query, inspect_slot, set_slot"), *Operation));
}

FMCPToolResult FMCPTool_WidgetBind::ExecuteCreate(const TSharedRef<FJsonObject>& Params)
{
	FString BPPath = ExtractOptionalString(Params, TEXT("blueprint_path"));
	FString PackagePath = ExtractOptionalString(Params, TEXT("package_path"), TEXT("/Game/UI/"));
	FString ParentClassStr = ExtractOptionalString(Params, TEXT("parent_class"));

	if (BPPath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: blueprint_path (name for new Widget Blueprint)"));
	}

	FString AssetName = FPaths::GetBaseFilename(BPPath);
	FString FullPackagePath;
	if (BPPath.StartsWith(TEXT("/Game/")))
	{
		FullPackagePath = FPaths::GetPath(BPPath);
		if (FullPackagePath.IsEmpty()) FullPackagePath = PackagePath;
	}
	else
	{
		FullPackagePath = PackagePath;
	}

	FString FullPath = FullPackagePath / AssetName;

	UClass* ParentClass = UUserWidget::StaticClass();
	if (!ParentClassStr.IsEmpty())
	{
		UClass* CustomParent = LoadClass<UUserWidget>(nullptr, *ParentClassStr);
		if (!CustomParent && !ParentClassStr.EndsWith(TEXT("_C")))
		{
			CustomParent = LoadClass<UUserWidget>(nullptr, *(ParentClassStr + TEXT("_C")));
		}
		if (CustomParent)
		{
			ParentClass = CustomParent;
		}
		else
		{
			return FMCPToolResult::Error(FString::Printf(
				TEXT("Parent class not found: %s (must derive from UUserWidget)"), *ParentClassStr));
		}
	}

	UPackage* Package = CreatePackage(*FullPath);
	if (!Package)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to create package: %s"), *FullPath));
	}

	UWidgetBlueprint* WidgetBP = CastChecked<UWidgetBlueprint>(
		FKismetEditorUtilities::CreateBlueprint(ParentClass, Package, FName(*AssetName),
			BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass()));

	if (!WidgetBP)
	{
		return FMCPToolResult::Error(TEXT("Failed to create Widget Blueprint"));
	}

	// Add a default CanvasPanel as root if widget tree is empty
	if (WidgetBP->WidgetTree)
	{
		UCanvasPanel* RootCanvas = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		if (RootCanvas)
		{
			WidgetBP->WidgetTree->RootWidget = RootCanvas;
		}
	}

	// If parent has BindWidget properties, create matching widgets
	TArray<FString> CreatedBindWidgets;
	if (ParentClass != UUserWidget::StaticClass())
	{
		for (TFieldIterator<FProperty> PropIt(ParentClass); PropIt; ++PropIt)
		{
			FProperty* Prop = *PropIt;
			if (!Prop->HasMetaData(TEXT("BindWidget")) && !Prop->HasMetaData(TEXT("BindWidgetOptional")))
			{
				continue;
			}

			FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop);
			if (!ObjProp) continue;

			UClass* WidgetClass = ObjProp->PropertyClass;
			if (!WidgetClass || !WidgetClass->IsChildOf(UWidget::StaticClass())) continue;

			FString WidgetName = Prop->GetName();

			if (WidgetBP->WidgetTree && WidgetBP->WidgetTree->RootWidget)
			{
				UWidget* NewWidget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*WidgetName));
				if (NewWidget)
				{
					UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
					if (RootPanel)
					{
						RootPanel->AddChild(NewWidget);
						CreatedBindWidgets.Add(FString::Printf(TEXT("%s (%s)"), *WidgetName, *WidgetClass->GetName()));
					}
				}
			}
		}
	}

	FKismetEditorUtilities::CompileBlueprint(WidgetBP);

	Package->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(FullPath, false);
	FAssetRegistryModule::AssetCreated(WidgetBP);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("asset_path"), WidgetBP->GetPathName());
	ResultData->SetStringField(TEXT("name"), AssetName);
	ResultData->SetStringField(TEXT("parent_class"), ParentClass->GetName());
	if (CreatedBindWidgets.Num() > 0)
	{
		ResultData->SetArrayField(TEXT("bind_widgets_created"), StringArrayToJsonArray(CreatedBindWidgets));
	}

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Created Widget Blueprint '%s' with %d BindWidget(s)"),
			*AssetName, CreatedBindWidgets.Num()),
		ResultData);
}

FMCPToolResult FMCPTool_WidgetBind::ExecuteAddWidget(const TSharedRef<FJsonObject>& Params)
{
	FString BPPath, WidgetName, WidgetTypeStr;
	TOptional<FMCPToolResult> Error;

	if (!ExtractRequiredString(Params, TEXT("blueprint_path"), BPPath, Error))
	{
		return Error.GetValue();
	}
	if (!ExtractRequiredString(Params, TEXT("widget_name"), WidgetName, Error))
	{
		return Error.GetValue();
	}
	if (!ExtractRequiredString(Params, TEXT("widget_type"), WidgetTypeStr, Error))
	{
		return Error.GetValue();
	}

	// Load widget blueprint
	FString AdjustedPath = BPPath;
	if (!AdjustedPath.Contains(TEXT(".")))
	{
		AdjustedPath += TEXT(".") + FPaths::GetBaseFilename(BPPath);
	}

	UWidgetBlueprint* WidgetBP = LoadObject<UWidgetBlueprint>(nullptr, *AdjustedPath);
	if (!WidgetBP)
	{
		WidgetBP = LoadObject<UWidgetBlueprint>(nullptr, *BPPath);
	}
	if (!WidgetBP)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Widget Blueprint not found: %s"), *BPPath));
	}

	// Resolve widget class
	static TMap<FString, UClass*> WidgetClassMap;
	if (WidgetClassMap.Num() == 0)
	{
		WidgetClassMap.Add(TEXT("textblock"), UTextBlock::StaticClass());
		WidgetClassMap.Add(TEXT("button"), UButton::StaticClass());
		WidgetClassMap.Add(TEXT("image"), UImage::StaticClass());
		WidgetClassMap.Add(TEXT("progressbar"), UProgressBar::StaticClass());
		WidgetClassMap.Add(TEXT("slider"), USlider::StaticClass());
		WidgetClassMap.Add(TEXT("checkbox"), UCheckBox::StaticClass());
		WidgetClassMap.Add(TEXT("editabletext"), UEditableTextBox::StaticClass());
		WidgetClassMap.Add(TEXT("richtextblock"), URichTextBlock::StaticClass());
		WidgetClassMap.Add(TEXT("border"), UBorder::StaticClass());
		WidgetClassMap.Add(TEXT("canvas"), UCanvasPanel::StaticClass());
		WidgetClassMap.Add(TEXT("canvaspanel"), UCanvasPanel::StaticClass());
		WidgetClassMap.Add(TEXT("horizontalbox"), UHorizontalBox::StaticClass());
		WidgetClassMap.Add(TEXT("verticalbox"), UVerticalBox::StaticClass());
		WidgetClassMap.Add(TEXT("gridpanel"), UGridPanel::StaticClass());
		WidgetClassMap.Add(TEXT("overlay"), UOverlay::StaticClass());
		WidgetClassMap.Add(TEXT("sizebox"), USizeBox::StaticClass());
		WidgetClassMap.Add(TEXT("scalebox"), UScaleBox::StaticClass());
		WidgetClassMap.Add(TEXT("spacer"), USpacer::StaticClass());
		WidgetClassMap.Add(TEXT("throbber"), UThrobber::StaticClass());
	}

	UClass** FoundClass = WidgetClassMap.Find(WidgetTypeStr.ToLower());
	if (!FoundClass)
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Unknown widget type: %s. Valid: TextBlock, Button, Image, ProgressBar, Slider, CheckBox, "
				"EditableText, RichTextBlock, Border, Canvas, HorizontalBox, VerticalBox, GridPanel, "
				"Overlay, SizeBox, ScaleBox, Spacer, Throbber"),
			*WidgetTypeStr));
	}

	if (!WidgetBP->WidgetTree)
	{
		return FMCPToolResult::Error(TEXT("Widget Blueprint has no WidgetTree"));
	}

	UWidget* NewWidget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(*FoundClass, FName(*WidgetName));
	if (!NewWidget)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to construct widget: %s"), *WidgetTypeStr));
	}

	// Find parent widget
	FString ParentWidgetName = ExtractOptionalString(Params, TEXT("parent_widget"));
	UPanelWidget* ParentPanel = nullptr;

	if (!ParentWidgetName.IsEmpty())
	{
		UWidget* Found = WidgetBP->WidgetTree->FindWidget(FName(*ParentWidgetName));
		ParentPanel = Cast<UPanelWidget>(Found);
		if (!ParentPanel)
		{
			return FMCPToolResult::Error(FString::Printf(
				TEXT("Parent widget '%s' not found or is not a panel widget"), *ParentWidgetName));
		}
	}
	else
	{
		ParentPanel = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
	}

	if (ParentPanel)
	{
		ParentPanel->AddChild(NewWidget);
	}
	else
	{
		WidgetBP->WidgetTree->RootWidget = NewWidget;
	}

	FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	WidgetBP->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(WidgetBP->GetOutermost()->GetPathName(), false);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("widget_name"), WidgetName);
	ResultData->SetStringField(TEXT("widget_type"), WidgetTypeStr);
	ResultData->SetStringField(TEXT("parent"), ParentPanel ? ParentPanel->GetName() : TEXT("Root"));
	ResultData->SetStringField(TEXT("blueprint_path"), WidgetBP->GetPathName());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added %s '%s' to '%s'"), *WidgetTypeStr, *WidgetName,
			ParentPanel ? *ParentPanel->GetName() : TEXT("Root")),
		ResultData);
}

FMCPToolResult FMCPTool_WidgetBind::ExecuteQuery(const TSharedRef<FJsonObject>& Params)
{
	FString BPPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("blueprint_path"), BPPath, Error))
	{
		return Error.GetValue();
	}

	FString AdjustedPath = BPPath;
	if (!AdjustedPath.Contains(TEXT(".")))
	{
		AdjustedPath += TEXT(".") + FPaths::GetBaseFilename(BPPath);
	}

	UWidgetBlueprint* WidgetBP = LoadObject<UWidgetBlueprint>(nullptr, *AdjustedPath);
	if (!WidgetBP)
	{
		WidgetBP = LoadObject<UWidgetBlueprint>(nullptr, *BPPath);
	}
	if (!WidgetBP)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Widget Blueprint not found: %s"), *BPPath));
	}

	TArray<TSharedPtr<FJsonValue>> WidgetArray;

	if (WidgetBP->WidgetTree)
	{
		WidgetBP->WidgetTree->ForEachWidget([&WidgetArray](UWidget* Widget) {
			if (!Widget) return;

			TSharedPtr<FJsonObject> WObj = MakeShared<FJsonObject>();
			WObj->SetStringField(TEXT("name"), Widget->GetName());
			WObj->SetStringField(TEXT("class"), Widget->GetClass()->GetName());
			WObj->SetBoolField(TEXT("is_panel"), Widget->IsA<UPanelWidget>());

			UPanelWidget* ParentPanel = Widget->GetParent();
			WObj->SetStringField(TEXT("parent"), ParentPanel ? ParentPanel->GetName() : TEXT("Root"));

			if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
			{
				WObj->SetNumberField(TEXT("child_count"), Panel->GetChildrenCount());
			}

			WidgetArray.Add(MakeShared<FJsonValueObject>(WObj));
		});
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("blueprint_path"), WidgetBP->GetPathName());
	ResultData->SetStringField(TEXT("parent_class"), WidgetBP->ParentClass ? WidgetBP->ParentClass->GetName() : TEXT("Unknown"));
	ResultData->SetNumberField(TEXT("widget_count"), WidgetArray.Num());
	ResultData->SetArrayField(TEXT("widgets"), WidgetArray);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Widget Blueprint '%s' has %d widgets"), *WidgetBP->GetName(), WidgetArray.Num()),
		ResultData);
}

// --------------------------------------------------------------------------
// Slot inspection / mutation helpers
// --------------------------------------------------------------------------

namespace
{
	// Walk the widget tree (root + every child) by name.
	UWidget* FindWidgetByName(UWidgetTree* Tree, const FString& Name)
	{
		if (!Tree) { return nullptr; }
		UWidget* Found = nullptr;
		Tree->ForEachWidget([&](UWidget* W)
		{
			if (!Found && W && W->GetName() == Name) { Found = W; }
		});
		return Found;
	}

	bool Vec2FromObj(const TSharedPtr<FJsonObject>& Obj, FVector2D& Out)
	{
		if (!Obj.IsValid()) { return false; }
		double X = Out.X, Y = Out.Y;
		const bool bX = Obj->TryGetNumberField(TEXT("x"), X);
		const bool bY = Obj->TryGetNumberField(TEXT("y"), Y);
		if (bX || bY) { Out = FVector2D(X, Y); return true; }
		return false;
	}

	bool MarginFromObj(const TSharedPtr<FJsonObject>& Obj, FMargin& Out)
	{
		if (!Obj.IsValid()) { return false; }
		double L = Out.Left, T = Out.Top, R = Out.Right, B = Out.Bottom;
		bool bAny = false;
		bAny |= Obj->TryGetNumberField(TEXT("left"),   L);
		bAny |= Obj->TryGetNumberField(TEXT("top"),    T);
		bAny |= Obj->TryGetNumberField(TEXT("right"),  R);
		bAny |= Obj->TryGetNumberField(TEXT("bottom"), B);
		if (bAny) { Out = FMargin(L, T, R, B); }
		return bAny;
	}

	bool ParseHAlign(const FString& S, EHorizontalAlignment& Out)
	{
		const FString L = S.ToLower();
		if (L == TEXT("left"))   { Out = HAlign_Left; return true; }
		if (L == TEXT("center") || L == TEXT("centre")) { Out = HAlign_Center; return true; }
		if (L == TEXT("right"))  { Out = HAlign_Right; return true; }
		if (L == TEXT("fill"))   { Out = HAlign_Fill; return true; }
		return false;
	}

	bool ParseVAlign(const FString& S, EVerticalAlignment& Out)
	{
		const FString L = S.ToLower();
		if (L == TEXT("top"))    { Out = VAlign_Top; return true; }
		if (L == TEXT("center") || L == TEXT("centre")) { Out = VAlign_Center; return true; }
		if (L == TEXT("bottom")) { Out = VAlign_Bottom; return true; }
		if (L == TEXT("fill"))   { Out = VAlign_Fill; return true; }
		return false;
	}

	const TCHAR* HAlignName(EHorizontalAlignment A)
	{
		switch (A) { case HAlign_Left: return TEXT("Left"); case HAlign_Center: return TEXT("Center");
		             case HAlign_Right: return TEXT("Right"); case HAlign_Fill: return TEXT("Fill"); }
		return TEXT("?");
	}
	const TCHAR* VAlignName(EVerticalAlignment A)
	{
		switch (A) { case VAlign_Top: return TEXT("Top"); case VAlign_Center: return TEXT("Center");
		             case VAlign_Bottom: return TEXT("Bottom"); case VAlign_Fill: return TEXT("Fill"); }
		return TEXT("?");
	}

	TSharedPtr<FJsonObject> MarginToJson(const FMargin& M)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("left"),   M.Left);
		O->SetNumberField(TEXT("top"),    M.Top);
		O->SetNumberField(TEXT("right"),  M.Right);
		O->SetNumberField(TEXT("bottom"), M.Bottom);
		return O;
	}

	TSharedPtr<FJsonObject> Vec2ToJson(const FVector2D& V)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("x"), V.X);
		O->SetNumberField(TEXT("y"), V.Y);
		return O;
	}
}

FMCPToolResult FMCPTool_WidgetBind::ExecuteInspectSlot(const TSharedRef<FJsonObject>& Params)
{
	FString BPPath, WidgetName;
	TOptional<FMCPToolResult> Err;
	if (!ExtractRequiredString(Params, TEXT("blueprint_path"), BPPath, Err)) return Err.GetValue();
	if (!ExtractRequiredString(Params, TEXT("widget_name"), WidgetName, Err)) return Err.GetValue();

	UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(BPPath));
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Widget Blueprint not found: %s"), *BPPath));
	}

	UWidget* W = FindWidgetByName(WidgetBP->WidgetTree, WidgetName);
	if (!W)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Widget '%s' not found in %s"), *WidgetName, *BPPath));
	}

	TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetStringField(TEXT("widget"), W->GetName());
	Out->SetStringField(TEXT("widget_class"), W->GetClass()->GetName());

	UPanelSlot* Slot = W->Slot;
	if (!Slot)
	{
		Out->SetStringField(TEXT("slot_class"), TEXT("(none — root or unparented)"));
		return FMCPToolResult::Success(TEXT("Widget has no slot (root or unparented)"), Out);
	}
	Out->SetStringField(TEXT("slot_class"), Slot->GetClass()->GetName());
	if (UWidget* Parent = Slot->Parent)
	{
		Out->SetStringField(TEXT("parent"), Parent->GetName());
		Out->SetStringField(TEXT("parent_class"), Parent->GetClass()->GetName());
	}

	if (UCanvasPanelSlot* C = Cast<UCanvasPanelSlot>(Slot))
	{
		const FAnchors A = C->GetAnchors();
		Out->SetObjectField(TEXT("anchor_min"), Vec2ToJson(A.Minimum));
		Out->SetObjectField(TEXT("anchor_max"), Vec2ToJson(A.Maximum));
		Out->SetObjectField(TEXT("alignment"), Vec2ToJson(C->GetAlignment()));
		Out->SetObjectField(TEXT("offsets"), MarginToJson(C->GetOffsets()));
		Out->SetBoolField(TEXT("auto_size"), C->GetAutoSize());
		Out->SetNumberField(TEXT("z_order"), C->GetZOrder());
	}
	else if (UHorizontalBoxSlot* H = Cast<UHorizontalBoxSlot>(Slot))
	{
		Out->SetObjectField(TEXT("padding"), MarginToJson(H->GetPadding()));
		Out->SetStringField(TEXT("h_align"), HAlignName(H->GetHorizontalAlignment()));
		Out->SetStringField(TEXT("v_align"), VAlignName(H->GetVerticalAlignment()));
		const FSlateChildSize Sz = H->GetSize();
		Out->SetStringField(TEXT("size_rule"), Sz.SizeRule == ESlateSizeRule::Fill ? TEXT("Fill") : TEXT("Auto"));
		Out->SetNumberField(TEXT("fill_value"), Sz.Value);
	}
	else if (UVerticalBoxSlot* V = Cast<UVerticalBoxSlot>(Slot))
	{
		Out->SetObjectField(TEXT("padding"), MarginToJson(V->GetPadding()));
		Out->SetStringField(TEXT("h_align"), HAlignName(V->GetHorizontalAlignment()));
		Out->SetStringField(TEXT("v_align"), VAlignName(V->GetVerticalAlignment()));
		const FSlateChildSize Sz = V->GetSize();
		Out->SetStringField(TEXT("size_rule"), Sz.SizeRule == ESlateSizeRule::Fill ? TEXT("Fill") : TEXT("Auto"));
		Out->SetNumberField(TEXT("fill_value"), Sz.Value);
	}
	else
	{
		Out->SetStringField(TEXT("note"), TEXT("slot type not yet inspectable; only CanvasPanelSlot/HBox/VBox slots fully supported"));
	}

	return FMCPToolResult::Success(FString::Printf(TEXT("Inspected slot of '%s'"), *WidgetName), Out);
}

FMCPToolResult FMCPTool_WidgetBind::ExecuteSetSlot(const TSharedRef<FJsonObject>& Params)
{
	FString BPPath, WidgetName;
	TOptional<FMCPToolResult> Err;
	if (!ExtractRequiredString(Params, TEXT("blueprint_path"), BPPath, Err)) return Err.GetValue();
	if (!ExtractRequiredString(Params, TEXT("widget_name"), WidgetName, Err)) return Err.GetValue();

	UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(BPPath));
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Widget Blueprint not found: %s"), *BPPath));
	}

	UWidget* W = FindWidgetByName(WidgetBP->WidgetTree, WidgetName);
	if (!W)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Widget '%s' not found in %s"), *WidgetName, *BPPath));
	}
	UPanelSlot* Slot = W->Slot;
	if (!Slot)
	{
		return FMCPToolResult::Error(TEXT("Widget has no slot (root or unparented)"));
	}

	TArray<FString> Changes;

	const TSharedPtr<FJsonObject>* TmpObj = nullptr;

	if (UCanvasPanelSlot* C = Cast<UCanvasPanelSlot>(Slot))
	{
		FAnchors A = C->GetAnchors();
		bool bAnchorChanged = false;
		if (Params->TryGetObjectField(TEXT("anchor_min"), TmpObj))
		{
			FVector2D Mn = A.Minimum; if (Vec2FromObj(*TmpObj, Mn)) { A.Minimum = Mn; bAnchorChanged = true; }
		}
		if (Params->TryGetObjectField(TEXT("anchor_max"), TmpObj))
		{
			FVector2D Mx = A.Maximum; if (Vec2FromObj(*TmpObj, Mx)) { A.Maximum = Mx; bAnchorChanged = true; }
		}
		if (bAnchorChanged) { C->SetAnchors(A); Changes.Add(TEXT("anchors")); }

		if (Params->TryGetObjectField(TEXT("alignment"), TmpObj))
		{
			FVector2D Al = C->GetAlignment();
			if (Vec2FromObj(*TmpObj, Al)) { C->SetAlignment(Al); Changes.Add(TEXT("alignment")); }
		}
		if (Params->TryGetObjectField(TEXT("offsets"), TmpObj))
		{
			FMargin Off = C->GetOffsets();
			if (MarginFromObj(*TmpObj, Off)) { C->SetOffsets(Off); Changes.Add(TEXT("offsets")); }
		}
		bool bAuto;
		if (Params->TryGetBoolField(TEXT("auto_size"), bAuto)) { C->SetAutoSize(bAuto); Changes.Add(TEXT("auto_size")); }
		double Z;
		if (Params->TryGetNumberField(TEXT("z_order"), Z)) { C->SetZOrder(static_cast<int32>(Z)); Changes.Add(TEXT("z_order")); }
	}
	else if (UHorizontalBoxSlot* H = Cast<UHorizontalBoxSlot>(Slot))
	{
		if (Params->TryGetObjectField(TEXT("padding"), TmpObj))
		{
			FMargin P = H->GetPadding();
			if (MarginFromObj(*TmpObj, P)) { H->SetPadding(P); Changes.Add(TEXT("padding")); }
		}
		FString HA;
		if (Params->TryGetStringField(TEXT("h_align"), HA))
		{
			EHorizontalAlignment E; if (ParseHAlign(HA, E)) { H->SetHorizontalAlignment(E); Changes.Add(TEXT("h_align")); }
		}
		FString VA;
		if (Params->TryGetStringField(TEXT("v_align"), VA))
		{
			EVerticalAlignment E; if (ParseVAlign(VA, E)) { H->SetVerticalAlignment(E); Changes.Add(TEXT("v_align")); }
		}
		FString SR;
		if (Params->TryGetStringField(TEXT("size_rule"), SR))
		{
			FSlateChildSize Sz = H->GetSize();
			const FString L = SR.ToLower();
			if (L == TEXT("auto")) { Sz.SizeRule = ESlateSizeRule::Automatic; H->SetSize(Sz); Changes.Add(TEXT("size_rule")); }
			else if (L == TEXT("fill")) { Sz.SizeRule = ESlateSizeRule::Fill; H->SetSize(Sz); Changes.Add(TEXT("size_rule")); }
		}
		double FV;
		if (Params->TryGetNumberField(TEXT("fill_value"), FV))
		{
			FSlateChildSize Sz = H->GetSize(); Sz.Value = static_cast<float>(FV); H->SetSize(Sz); Changes.Add(TEXT("fill_value"));
		}
	}
	else if (UVerticalBoxSlot* V = Cast<UVerticalBoxSlot>(Slot))
	{
		if (Params->TryGetObjectField(TEXT("padding"), TmpObj))
		{
			FMargin P = V->GetPadding();
			if (MarginFromObj(*TmpObj, P)) { V->SetPadding(P); Changes.Add(TEXT("padding")); }
		}
		FString HA;
		if (Params->TryGetStringField(TEXT("h_align"), HA))
		{
			EHorizontalAlignment E; if (ParseHAlign(HA, E)) { V->SetHorizontalAlignment(E); Changes.Add(TEXT("h_align")); }
		}
		FString VA;
		if (Params->TryGetStringField(TEXT("v_align"), VA))
		{
			EVerticalAlignment E; if (ParseVAlign(VA, E)) { V->SetVerticalAlignment(E); Changes.Add(TEXT("v_align")); }
		}
		FString SR;
		if (Params->TryGetStringField(TEXT("size_rule"), SR))
		{
			FSlateChildSize Sz = V->GetSize();
			const FString L = SR.ToLower();
			if (L == TEXT("auto")) { Sz.SizeRule = ESlateSizeRule::Automatic; V->SetSize(Sz); Changes.Add(TEXT("size_rule")); }
			else if (L == TEXT("fill")) { Sz.SizeRule = ESlateSizeRule::Fill; V->SetSize(Sz); Changes.Add(TEXT("size_rule")); }
		}
		double FV;
		if (Params->TryGetNumberField(TEXT("fill_value"), FV))
		{
			FSlateChildSize Sz = V->GetSize(); Sz.Value = static_cast<float>(FV); V->SetSize(Sz); Changes.Add(TEXT("fill_value"));
		}
	}
	else
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("set_slot: slot type '%s' not supported (CanvasPanelSlot, HorizontalBoxSlot, VerticalBoxSlot only)"),
			*Slot->GetClass()->GetName()));
	}

	if (Changes.Num() == 0)
	{
		return FMCPToolResult::Error(TEXT("set_slot: no recognised layout fields provided"));
	}

	// Mark dirty + save the WBP package so the changes persist.
	WidgetBP->Modify();
	WidgetBP->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	UEditorAssetLibrary::SaveLoadedAsset(WidgetBP);

	TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetStringField(TEXT("widget"), WidgetName);
	Out->SetStringField(TEXT("slot_class"), Slot->GetClass()->GetName());
	TArray<TSharedPtr<FJsonValue>> ArrJson;
	for (const FString& S : Changes) { ArrJson.Add(MakeShared<FJsonValueString>(S)); }
	Out->SetArrayField(TEXT("changed"), ArrJson);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Updated slot of '%s' (%d field%s)"), *WidgetName, Changes.Num(), Changes.Num() == 1 ? TEXT("") : TEXT("s")),
		Out);
}
