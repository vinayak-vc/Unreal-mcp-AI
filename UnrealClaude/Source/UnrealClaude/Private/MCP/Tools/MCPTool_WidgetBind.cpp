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

#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"

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

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: %s. Valid: create, add_widget, query"), *Operation));
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
