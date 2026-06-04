// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_SetClassDefaults.h"
#include "UnrealClaudeModule.h"
#include "MCP/MCPParamValidator.h"

#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EditorAssetLibrary.h"

FMCPToolResult FMCPTool_SetClassDefaults::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	if (Operation == TEXT("get_defaults"))
	{
		return ExecuteGetDefaults(Params);
	}
	if (Operation == TEXT("set_defaults"))
	{
		return ExecuteSetDefaults(Params);
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: %s. Valid: get_defaults, set_defaults"), *Operation));
}

UBlueprint* FMCPTool_SetClassDefaults::LoadBlueprint(const FString& Path, FString& OutError)
{
	if (!FMCPParamValidator::ValidateBlueprintPath(Path, OutError))
	{
		return nullptr;
	}

	FString AdjustedPath = Path;
	if (!AdjustedPath.Contains(TEXT(".")))
	{
		AdjustedPath += TEXT(".") + FPaths::GetBaseFilename(Path);
	}

	UObject* Asset = LoadObject<UObject>(nullptr, *AdjustedPath);
	if (!Asset)
	{
		Asset = LoadObject<UObject>(nullptr, *Path);
	}

	UBlueprint* BP = Cast<UBlueprint>(Asset);
	if (!BP)
	{
		OutError = FString::Printf(TEXT("Blueprint not found: %s"), *Path);
	}
	return BP;
}

FMCPToolResult FMCPTool_SetClassDefaults::ExecuteGetDefaults(const TSharedRef<FJsonObject>& Params)
{
	FString BlueprintPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("blueprint_path"), BlueprintPath, Error))
	{
		return Error.GetValue();
	}

	FString LoadError;
	UBlueprint* BP = LoadBlueprint(BlueprintPath, LoadError);
	if (!BP)
	{
		return FMCPToolResult::Error(LoadError);
	}

	UObject* CDO = BP->GeneratedClass ? BP->GeneratedClass->GetDefaultObject() : nullptr;
	if (!CDO)
	{
		return FMCPToolResult::Error(TEXT("Blueprint has no generated class or CDO. Compile the Blueprint first."));
	}

	bool bIncludeInherited = ExtractOptionalBool(Params, TEXT("include_inherited"), false);

	// Check if specific property names requested
	TSet<FString> RequestedNames;
	const TArray<TSharedPtr<FJsonValue>>* PropertyNamesArray;
	if (Params->TryGetArrayField(TEXT("property_names"), PropertyNamesArray))
	{
		for (const auto& Val : *PropertyNamesArray)
		{
			FString Name;
			if (Val->TryGetString(Name))
			{
				RequestedNames.Add(Name);
			}
		}
	}

	TSharedPtr<FJsonObject> PropertiesObj = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> PropertyList;

	UClass* TargetClass = BP->GeneratedClass;
	UClass* StopClass = bIncludeInherited ? UObject::StaticClass() : TargetClass->GetSuperClass();

	for (TFieldIterator<FProperty> PropIt(TargetClass, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;

		if (!bIncludeInherited && Prop->GetOwnerClass() != TargetClass)
		{
			continue;
		}

		if (!RequestedNames.IsEmpty() && !RequestedNames.Contains(Prop->GetName()))
		{
			continue;
		}

		if (!Prop->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
		{
			continue;
		}

		TSharedPtr<FJsonObject> PropInfo = MakeShared<FJsonObject>();
		PropInfo->SetStringField(TEXT("name"), Prop->GetName());
		PropInfo->SetStringField(TEXT("type"), Prop->GetCPPType());
		PropInfo->SetStringField(TEXT("category"), Prop->GetMetaData(TEXT("Category")));

		TSharedPtr<FJsonValue> Value = GetPropertyValue(CDO, Prop);
		if (Value.IsValid())
		{
			PropInfo->SetField(TEXT("value"), Value);
			PropertiesObj->SetField(Prop->GetName(), Value);
		}

		PropInfo->SetBoolField(TEXT("editable"), Prop->HasAnyPropertyFlags(CPF_Edit));
		PropInfo->SetBoolField(TEXT("blueprint_visible"), Prop->HasAnyPropertyFlags(CPF_BlueprintVisible));
		PropInfo->SetStringField(TEXT("owner_class"), Prop->GetOwnerClass()->GetName());

		PropertyList.Add(MakeShared<FJsonValueObject>(PropInfo));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("blueprint_path"), BP->GetPathName());
	ResultData->SetStringField(TEXT("blueprint_name"), BP->GetName());
	ResultData->SetStringField(TEXT("generated_class"), TargetClass->GetName());
	ResultData->SetNumberField(TEXT("property_count"), PropertyList.Num());
	ResultData->SetObjectField(TEXT("values"), PropertiesObj);
	ResultData->SetArrayField(TEXT("properties"), PropertyList);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Retrieved %d defaults from '%s'"), PropertyList.Num(), *BP->GetName()),
		ResultData);
}

FMCPToolResult FMCPTool_SetClassDefaults::ExecuteSetDefaults(const TSharedRef<FJsonObject>& Params)
{
	FString BlueprintPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("blueprint_path"), BlueprintPath, Error))
	{
		return Error.GetValue();
	}

	const TSharedPtr<FJsonObject>* PropertiesObj;
	if (!Params->TryGetObjectField(TEXT("properties"), PropertiesObj) || !PropertiesObj || !(*PropertiesObj).IsValid())
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: properties (JSON object of name->value pairs)"));
	}

	FString LoadError;
	UBlueprint* BP = LoadBlueprint(BlueprintPath, LoadError);
	if (!BP)
	{
		return FMCPToolResult::Error(LoadError);
	}

	UObject* CDO = BP->GeneratedClass ? BP->GeneratedClass->GetDefaultObject() : nullptr;
	if (!CDO)
	{
		return FMCPToolResult::Error(TEXT("Blueprint has no generated class or CDO. Compile first."));
	}

	TArray<FString> SetFields;
	TArray<FString> FailedFields;

	CDO->Modify();

	for (const auto& Pair : (*PropertiesObj)->Values)
	{
		FProperty* Prop = BP->GeneratedClass->FindPropertyByName(FName(*Pair.Key));
		if (!Prop)
		{
			Prop = FindFProperty<FProperty>(BP->GeneratedClass, FName(*Pair.Key));
		}
		if (!Prop)
		{
			FailedFields.Add(FString::Printf(TEXT("%s: property not found"), *Pair.Key));
			continue;
		}

		FString SetError;
		if (SetPropertyValue(CDO, Prop, Pair.Value, SetError))
		{
			SetFields.Add(Pair.Key);
		}
		else
		{
			FailedFields.Add(FString::Printf(TEXT("%s: %s"), *Pair.Key, *SetError));
		}
	}

	if (SetFields.Num() > 0)
	{
		CDO->MarkPackageDirty();
		BP->MarkPackageDirty();
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

		FString PackagePath = BP->GetOutermost()->GetPathName();
		UEditorAssetLibrary::SaveAsset(PackagePath, false);
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("blueprint_path"), BP->GetPathName());
	ResultData->SetNumberField(TEXT("set_count"), SetFields.Num());
	ResultData->SetArrayField(TEXT("set_properties"), StringArrayToJsonArray(SetFields));
	if (FailedFields.Num() > 0)
	{
		ResultData->SetArrayField(TEXT("failed_properties"), StringArrayToJsonArray(FailedFields));
	}

	if (SetFields.Num() == 0)
	{
		return FMCPToolResult::Error(
			FString::Printf(TEXT("No properties were set. Errors: %s"), *FString::Join(FailedFields, TEXT("; "))));
	}

	FMCPToolResult Result = FMCPToolResult::Success(
		FString::Printf(TEXT("Set %d defaults on '%s'"), SetFields.Num(), *BP->GetName()),
		ResultData);

	if (FailedFields.Num() > 0)
	{
		Result.Warnings.Add(FString::Printf(TEXT("%d properties failed: %s"),
			FailedFields.Num(), *FString::Join(FailedFields, TEXT("; "))));
	}
	return Result;
}

bool FMCPTool_SetClassDefaults::SetPropertyValue(UObject* CDO, FProperty* Prop, const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
	if (!CDO || !Prop || !Value.IsValid())
	{
		OutError = TEXT("null CDO, property, or value");
		return false;
	}

	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(CDO);

	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
	{
		bool bVal;
		if (Value->TryGetBool(bVal))
		{
			BoolProp->SetPropertyValue(ValuePtr, bVal);
			return true;
		}
		OutError = TEXT("expected boolean");
		return false;
	}

	if (FNumericProperty* NumProp = CastField<FNumericProperty>(Prop))
	{
		double NumVal;
		if (Value->TryGetNumber(NumVal))
		{
			if (NumProp->IsInteger())
			{
				NumProp->SetIntPropertyValue(ValuePtr, static_cast<int64>(NumVal));
			}
			else
			{
				NumProp->SetFloatingPointPropertyValue(ValuePtr, NumVal);
			}
			return true;
		}
		OutError = TEXT("expected number");
		return false;
	}

	if (FStrProperty* StrProp = CastField<FStrProperty>(Prop))
	{
		FString StrVal;
		if (Value->TryGetString(StrVal))
		{
			StrProp->SetPropertyValue(ValuePtr, StrVal);
			return true;
		}
		OutError = TEXT("expected string");
		return false;
	}

	if (FNameProperty* NameProp = CastField<FNameProperty>(Prop))
	{
		FString StrVal;
		if (Value->TryGetString(StrVal))
		{
			NameProp->SetPropertyValue(ValuePtr, FName(*StrVal));
			return true;
		}
		OutError = TEXT("expected string for FName");
		return false;
	}

	if (FTextProperty* TextProp = CastField<FTextProperty>(Prop))
	{
		FString StrVal;
		if (Value->TryGetString(StrVal))
		{
			TextProp->SetPropertyValue(ValuePtr, FText::FromString(StrVal));
			return true;
		}
		OutError = TEXT("expected string for FText");
		return false;
	}

	if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
	{
		const TSharedPtr<FJsonObject>* ObjVal;
		if (Value->TryGetObject(ObjVal) && ObjVal && (*ObjVal).IsValid())
		{
			FName StructName = StructProp->Struct->GetFName();

			if (StructName == NAME_Vector || StructName == NAME_Vector3f)
			{
				FVector Vec = UnrealClaudeJsonUtils::ExtractVector(*ObjVal, FVector::ZeroVector);
				StructProp->CopyCompleteValue(ValuePtr, &Vec);
				return true;
			}
			if (StructName == NAME_Rotator || StructName == NAME_Rotator3f)
			{
				FRotator Rot = UnrealClaudeJsonUtils::ExtractRotator(*ObjVal, FRotator::ZeroRotator);
				StructProp->CopyCompleteValue(ValuePtr, &Rot);
				return true;
			}
			if (StructName == NAME_Color)
			{
				double R, G, B, A;
				(*ObjVal)->TryGetNumberField(TEXT("r"), R);
				(*ObjVal)->TryGetNumberField(TEXT("g"), G);
				(*ObjVal)->TryGetNumberField(TEXT("b"), B);
				A = 255;
				(*ObjVal)->TryGetNumberField(TEXT("a"), A);
				FColor Color(static_cast<uint8>(R), static_cast<uint8>(G), static_cast<uint8>(B), static_cast<uint8>(A));
				StructProp->CopyCompleteValue(ValuePtr, &Color);
				return true;
			}
			if (StructName == NAME_LinearColor)
			{
				double R = 0, G = 0, B = 0, A = 1;
				(*ObjVal)->TryGetNumberField(TEXT("r"), R);
				(*ObjVal)->TryGetNumberField(TEXT("g"), G);
				(*ObjVal)->TryGetNumberField(TEXT("b"), B);
				(*ObjVal)->TryGetNumberField(TEXT("a"), A);
				FLinearColor LColor(static_cast<float>(R), static_cast<float>(G), static_cast<float>(B), static_cast<float>(A));
				StructProp->CopyCompleteValue(ValuePtr, &LColor);
				return true;
			}

			// Generic struct: try ImportText from JSON serialization
			FString JsonStr;
			auto Writer = TJsonWriterFactory<>::Create(&JsonStr);
			FJsonSerializer::Serialize((*ObjVal).ToSharedRef(), Writer);
			const TCHAR* ImportResult = Prop->ImportText_Direct(*JsonStr, ValuePtr, CDO, 0);
			if (ImportResult)
			{
				return true;
			}
			OutError = FString::Printf(TEXT("failed to parse struct '%s' from JSON"), *StructName.ToString());
			return false;
		}

		// Try string form (e.g. "(X=1,Y=2,Z=3)")
		FString StrVal;
		if (Value->TryGetString(StrVal))
		{
			const TCHAR* ImportResult = Prop->ImportText_Direct(*StrVal, ValuePtr, CDO, 0);
			if (ImportResult)
			{
				return true;
			}
			OutError = TEXT("failed to parse struct from string");
			return false;
		}
		OutError = TEXT("expected object or string for struct");
		return false;
	}

	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
	{
		FString StrVal;
		if (Value->TryGetString(StrVal))
		{
			int64 EnumVal = EnumProp->GetEnum()->GetValueByNameString(StrVal);
			if (EnumVal == INDEX_NONE)
			{
				OutError = FString::Printf(TEXT("invalid enum value: %s"), *StrVal);
				return false;
			}
			EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, EnumVal);
			return true;
		}
		double NumVal;
		if (Value->TryGetNumber(NumVal))
		{
			EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, static_cast<int64>(NumVal));
			return true;
		}
		OutError = TEXT("expected string or number for enum");
		return false;
	}

	if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
	{
		if (ByteProp->Enum)
		{
			FString StrVal;
			if (Value->TryGetString(StrVal))
			{
				int64 EnumIdx = ByteProp->Enum->GetValueByNameString(StrVal);
				if (EnumIdx != INDEX_NONE)
				{
					ByteProp->SetIntPropertyValue(ValuePtr, EnumIdx);
					return true;
				}
				OutError = FString::Printf(TEXT("invalid enum value: %s"), *StrVal);
				return false;
			}
		}
		double NumVal;
		if (Value->TryGetNumber(NumVal))
		{
			ByteProp->SetIntPropertyValue(ValuePtr, static_cast<int64>(NumVal));
			return true;
		}
		OutError = TEXT("expected number or enum string");
		return false;
	}

	if (CastField<FSoftObjectProperty>(Prop) || CastField<FSoftClassProperty>(Prop))
	{
		FString StrVal;
		if (Value->TryGetString(StrVal))
		{
			const TCHAR* Result = Prop->ImportText_Direct(*StrVal, ValuePtr, CDO, 0);
			if (Result) return true;
			OutError = TEXT("failed to parse soft reference path");
			return false;
		}
		OutError = TEXT("expected string path for soft reference");
		return false;
	}

	// Fallback: use ImportText_Direct
	FString StrVal;
	if (Value->TryGetString(StrVal))
	{
		const TCHAR* ImportResult = Prop->ImportText_Direct(*StrVal, ValuePtr, CDO, 0);
		if (ImportResult)
		{
			return true;
		}
	}

	OutError = FString::Printf(TEXT("unsupported property type: %s"), *Prop->GetCPPType());
	return false;
}

TSharedPtr<FJsonValue> FMCPTool_SetClassDefaults::GetPropertyValue(UObject* CDO, FProperty* Prop)
{
	if (!CDO || !Prop)
	{
		return MakeShared<FJsonValueNull>();
	}

	const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(CDO);

	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
	{
		return MakeShared<FJsonValueBoolean>(BoolProp->GetPropertyValue(ValuePtr));
	}

	if (FNumericProperty* NumProp = CastField<FNumericProperty>(Prop))
	{
		if (NumProp->IsInteger())
		{
			return MakeShared<FJsonValueNumber>(static_cast<double>(NumProp->GetSignedIntPropertyValue(ValuePtr)));
		}
		return MakeShared<FJsonValueNumber>(NumProp->GetFloatingPointPropertyValue(ValuePtr));
	}

	if (FStrProperty* StrProp = CastField<FStrProperty>(Prop))
	{
		return MakeShared<FJsonValueString>(StrProp->GetPropertyValue(ValuePtr));
	}

	if (FNameProperty* NameProp = CastField<FNameProperty>(Prop))
	{
		return MakeShared<FJsonValueString>(NameProp->GetPropertyValue(ValuePtr).ToString());
	}

	if (FTextProperty* TextProp = CastField<FTextProperty>(Prop))
	{
		return MakeShared<FJsonValueString>(TextProp->GetPropertyValue(ValuePtr).ToString());
	}

	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
	{
		int64 Val = EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
		FString EnumStr = EnumProp->GetEnum()->GetNameStringByValue(Val);
		return MakeShared<FJsonValueString>(EnumStr);
	}

	// Fallback: ExportText
	FString ExportedValue;
	Prop->ExportTextItem_Direct(ExportedValue, ValuePtr, nullptr, CDO, PPF_None);
	return MakeShared<FJsonValueString>(ExportedValue);
}
