// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_DataTable.h"
#include "UnrealClaudeModule.h"
#include "MCP/MCPParamValidator.h"

#include "Engine/DataTable.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"

FMCPToolResult FMCPTool_DataTable::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	if (Operation == TEXT("create")) return ExecuteCreate(Params);
	if (Operation == TEXT("get_rows")) return ExecuteGetRows(Params);
	if (Operation == TEXT("add_row")) return ExecuteAddRow(Params);
	if (Operation == TEXT("modify_row")) return ExecuteModifyRow(Params);
	if (Operation == TEXT("delete_row")) return ExecuteDeleteRow(Params);
	if (Operation == TEXT("query")) return ExecuteQuery(Params);

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: %s. Valid: create, get_rows, add_row, modify_row, delete_row, query"),
		*Operation));
}

UDataTable* FMCPTool_DataTable::LoadDataTable(const FString& Path, FString& OutError)
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

	UDataTable* Table = LoadObject<UDataTable>(nullptr, *AdjustedPath);
	if (!Table)
	{
		Table = LoadObject<UDataTable>(nullptr, *Path);
	}
	if (!Table)
	{
		OutError = FString::Printf(TEXT("DataTable not found: %s"), *Path);
	}
	return Table;
}

FMCPToolResult FMCPTool_DataTable::ExecuteCreate(const TSharedRef<FJsonObject>& Params)
{
	FString TableName;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("table_name"), TableName, Error))
	{
		return Error.GetValue();
	}

	FString RowStructName = ExtractOptionalString(Params, TEXT("row_struct"), TEXT("TableRowBase"));
	FString PackagePath = ExtractOptionalString(Params, TEXT("package_path"), TEXT("/Game/Data/"));

	UScriptStruct* RowStruct = FindObject<UScriptStruct>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *RowStructName));
	if (!RowStruct)
	{
		RowStruct = FindFirstObject<UScriptStruct>(*RowStructName, EFindFirstObjectOptions::NativeFirst);
	}
	if (!RowStruct)
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Row struct not found: %s. Common structs: TableRowBase, DataTableRowHandle"), *RowStructName));
	}

	FString FullPath = PackagePath / TableName;
	UPackage* Package = CreatePackage(*FullPath);
	if (!Package)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to create package: %s"), *FullPath));
	}

	UDataTable* NewTable = NewObject<UDataTable>(Package, FName(*TableName), RF_Public | RF_Standalone);
	if (!NewTable)
	{
		return FMCPToolResult::Error(TEXT("Failed to create DataTable"));
	}

	NewTable->RowStruct = RowStruct;
	Package->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(FullPath, false);
	FAssetRegistryModule::AssetCreated(NewTable);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("asset_path"), NewTable->GetPathName());
	ResultData->SetStringField(TEXT("name"), TableName);
	ResultData->SetStringField(TEXT("row_struct"), RowStruct->GetName());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Created DataTable '%s' with struct '%s'"), *TableName, *RowStruct->GetName()),
		ResultData);
}

FMCPToolResult FMCPTool_DataTable::ExecuteQuery(const TSharedRef<FJsonObject>& Params)
{
	FString TablePath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("table_path"), TablePath, Error))
	{
		return Error.GetValue();
	}

	FString LoadError;
	UDataTable* Table = LoadDataTable(TablePath, LoadError);
	if (!Table)
	{
		return FMCPToolResult::Error(LoadError);
	}

	TArray<FName> RowNames = Table->GetRowNames();

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("table_path"), Table->GetPathName());
	ResultData->SetStringField(TEXT("row_struct"), Table->RowStruct ? Table->RowStruct->GetName() : TEXT("None"));
	ResultData->SetNumberField(TEXT("row_count"), RowNames.Num());

	TArray<FString> NameStrs;
	for (const FName& N : RowNames)
	{
		NameStrs.Add(N.ToString());
	}
	ResultData->SetArrayField(TEXT("row_names"), StringArrayToJsonArray(NameStrs));

	// List struct fields
	if (Table->RowStruct)
	{
		TArray<TSharedPtr<FJsonValue>> Fields;
		for (TFieldIterator<FProperty> It(Table->RowStruct); It; ++It)
		{
			TSharedPtr<FJsonObject> FieldObj = MakeShared<FJsonObject>();
			FieldObj->SetStringField(TEXT("name"), It->GetName());
			FieldObj->SetStringField(TEXT("type"), It->GetCPPType());
			Fields.Add(MakeShared<FJsonValueObject>(FieldObj));
		}
		ResultData->SetArrayField(TEXT("fields"), Fields);
	}

	return FMCPToolResult::Success(
		FString::Printf(TEXT("DataTable '%s': %d rows"), *Table->GetName(), RowNames.Num()),
		ResultData);
}

TSharedPtr<FJsonObject> FMCPTool_DataTable::RowToJson(UDataTable* Table, const FName& RowName)
{
	TSharedPtr<FJsonObject> RowObj = MakeShared<FJsonObject>();
	RowObj->SetStringField(TEXT("row_name"), RowName.ToString());

	if (!Table || !Table->RowStruct)
	{
		return RowObj;
	}

	uint8* RowData = Table->FindRowUnchecked(RowName);
	if (!RowData)
	{
		return RowObj;
	}

	for (TFieldIterator<FProperty> It(Table->RowStruct); It; ++It)
	{
		FProperty* Prop = *It;
		const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(RowData);
		FString ExportedValue;
		Prop->ExportTextItem_Direct(ExportedValue, ValuePtr, nullptr, nullptr, PPF_None);
		RowObj->SetStringField(Prop->GetName(), ExportedValue);
	}

	return RowObj;
}

FMCPToolResult FMCPTool_DataTable::ExecuteGetRows(const TSharedRef<FJsonObject>& Params)
{
	FString TablePath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("table_path"), TablePath, Error))
	{
		return Error.GetValue();
	}

	FString LoadError;
	UDataTable* Table = LoadDataTable(TablePath, LoadError);
	if (!Table)
	{
		return FMCPToolResult::Error(LoadError);
	}

	FString RowName = ExtractOptionalString(Params, TEXT("row_name"));

	TArray<TSharedPtr<FJsonValue>> Rows;
	if (!RowName.IsEmpty())
	{
		TSharedPtr<FJsonObject> Row = RowToJson(Table, FName(*RowName));
		Rows.Add(MakeShared<FJsonValueObject>(Row));
	}
	else
	{
		for (const FName& Name : Table->GetRowNames())
		{
			Rows.Add(MakeShared<FJsonValueObject>(RowToJson(Table, Name)));
		}
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetNumberField(TEXT("count"), Rows.Num());
	ResultData->SetArrayField(TEXT("rows"), Rows);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Retrieved %d rows from '%s'"), Rows.Num(), *Table->GetName()),
		ResultData);
}

FMCPToolResult FMCPTool_DataTable::ExecuteAddRow(const TSharedRef<FJsonObject>& Params)
{
	FString TablePath, RowName;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("table_path"), TablePath, Error)) return Error.GetValue();
	if (!ExtractRequiredString(Params, TEXT("row_name"), RowName, Error)) return Error.GetValue();

	const TSharedPtr<FJsonObject>* RowDataObj;
	if (!Params->TryGetObjectField(TEXT("row_data"), RowDataObj))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: row_data"));
	}

	FString LoadError;
	UDataTable* Table = LoadDataTable(TablePath, LoadError);
	if (!Table) return FMCPToolResult::Error(LoadError);

	if (Table->FindRowUnchecked(FName(*RowName)))
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Row '%s' already exists. Use modify_row instead."), *RowName));
	}

	// Allocate a temporary row struct, populate it, then add to table
	uint8* TempRowData = static_cast<uint8*>(FMemory::Malloc(Table->RowStruct->GetStructureSize()));
	Table->RowStruct->InitializeStruct(TempRowData);

	for (const auto& Pair : (*RowDataObj)->Values)
	{
		FProperty* Prop = FindFProperty<FProperty>(Table->RowStruct, FName(*Pair.Key));
		if (!Prop) continue;

		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(TempRowData);
		FString StrVal;
		if (Pair.Value->TryGetString(StrVal))
		{
			Prop->ImportText_Direct(*StrVal, ValuePtr, nullptr, 0);
		}
		else
		{
			double NumVal;
			if (Pair.Value->TryGetNumber(NumVal))
			{
				if (FNumericProperty* NumProp = CastField<FNumericProperty>(Prop))
				{
					if (NumProp->IsInteger())
						NumProp->SetIntPropertyValue(ValuePtr, static_cast<int64>(NumVal));
					else
						NumProp->SetFloatingPointPropertyValue(ValuePtr, NumVal);
				}
			}
			bool BoolVal;
			if (Pair.Value->TryGetBool(BoolVal))
			{
				if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
				{
					BoolProp->SetPropertyValue(ValuePtr, BoolVal);
				}
			}
		}
	}

	Table->AddRow(FName(*RowName), *reinterpret_cast<FTableRowBase*>(TempRowData));

	Table->RowStruct->DestroyStruct(TempRowData);
	FMemory::Free(TempRowData);

	Table->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(Table->GetOutermost()->GetPathName(), false);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("row_name"), RowName);
	ResultData->SetNumberField(TEXT("total_rows"), Table->GetRowNames().Num());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added row '%s' to '%s'"), *RowName, *Table->GetName()),
		ResultData);
}

FMCPToolResult FMCPTool_DataTable::ExecuteModifyRow(const TSharedRef<FJsonObject>& Params)
{
	FString TablePath, RowName;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("table_path"), TablePath, Error)) return Error.GetValue();
	if (!ExtractRequiredString(Params, TEXT("row_name"), RowName, Error)) return Error.GetValue();

	const TSharedPtr<FJsonObject>* RowDataObj;
	if (!Params->TryGetObjectField(TEXT("row_data"), RowDataObj))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: row_data"));
	}

	FString LoadError;
	UDataTable* Table = LoadDataTable(TablePath, LoadError);
	if (!Table) return FMCPToolResult::Error(LoadError);

	uint8* RowData = Table->FindRowUnchecked(FName(*RowName));
	if (!RowData)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Row '%s' not found"), *RowName));
	}

	TArray<FString> ModifiedFields;
	for (const auto& Pair : (*RowDataObj)->Values)
	{
		FProperty* Prop = FindFProperty<FProperty>(Table->RowStruct, FName(*Pair.Key));
		if (!Prop) continue;

		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(RowData);
		FString StrVal;
		if (Pair.Value->TryGetString(StrVal))
		{
			Prop->ImportText_Direct(*StrVal, ValuePtr, nullptr, 0);
			ModifiedFields.Add(Pair.Key);
		}
		else
		{
			double NumVal;
			if (Pair.Value->TryGetNumber(NumVal))
			{
				if (FNumericProperty* NumProp = CastField<FNumericProperty>(Prop))
				{
					if (NumProp->IsInteger())
						NumProp->SetIntPropertyValue(ValuePtr, static_cast<int64>(NumVal));
					else
						NumProp->SetFloatingPointPropertyValue(ValuePtr, NumVal);
					ModifiedFields.Add(Pair.Key);
				}
			}
			bool BoolVal;
			if (Pair.Value->TryGetBool(BoolVal))
			{
				if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
				{
					BoolProp->SetPropertyValue(ValuePtr, BoolVal);
					ModifiedFields.Add(Pair.Key);
				}
			}
		}
	}

	Table->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(Table->GetOutermost()->GetPathName(), false);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("row_name"), RowName);
	ResultData->SetArrayField(TEXT("modified_fields"), StringArrayToJsonArray(ModifiedFields));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Modified %d fields in row '%s'"), ModifiedFields.Num(), *RowName),
		ResultData);
}

FMCPToolResult FMCPTool_DataTable::ExecuteDeleteRow(const TSharedRef<FJsonObject>& Params)
{
	FString TablePath, RowName;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("table_path"), TablePath, Error)) return Error.GetValue();
	if (!ExtractRequiredString(Params, TEXT("row_name"), RowName, Error)) return Error.GetValue();

	FString LoadError;
	UDataTable* Table = LoadDataTable(TablePath, LoadError);
	if (!Table) return FMCPToolResult::Error(LoadError);

	if (!Table->FindRowUnchecked(FName(*RowName)))
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Row '%s' not found"), *RowName));
	}

	Table->RemoveRow(FName(*RowName));
	Table->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(Table->GetOutermost()->GetPathName(), false);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("deleted_row"), RowName);
	ResultData->SetNumberField(TEXT("remaining_rows"), Table->GetRowNames().Num());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Deleted row '%s' from '%s'"), *RowName, *Table->GetName()),
		ResultData);
}
