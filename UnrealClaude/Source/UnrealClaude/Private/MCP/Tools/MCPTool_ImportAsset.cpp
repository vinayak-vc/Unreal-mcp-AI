// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_ImportAsset.h"
#include "UnrealClaudeModule.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AutomatedAssetImportData.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

FMCPToolResult FMCPTool_ImportAsset::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	if (Operation == TEXT("import"))
	{
		return ExecuteImport(Params);
	}
	if (Operation == TEXT("bulk_import"))
	{
		return ExecuteBulkImport(Params);
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: %s. Valid: import, bulk_import"), *Operation));
}

FMCPToolResult FMCPTool_ImportAsset::ExecuteImport(const TSharedRef<FJsonObject>& Params)
{
	FString SourcePath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("source_path"), SourcePath, Error))
	{
		return Error.GetValue();
	}

	FString DestPath = ExtractOptionalString(Params, TEXT("destination_path"), TEXT("/Game/Imported/"));
	bool bReplace = ExtractOptionalBool(Params, TEXT("replace_existing"), true);

	FString ImportError;
	TSharedPtr<FJsonObject> ResultData = ImportSingleFile(SourcePath, DestPath, bReplace, ImportError);

	if (!ResultData.IsValid())
	{
		return FMCPToolResult::Error(ImportError);
	}

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Imported '%s' to '%s'"), *FPaths::GetCleanFilename(SourcePath), *DestPath),
		ResultData);
}

FMCPToolResult FMCPTool_ImportAsset::ExecuteBulkImport(const TSharedRef<FJsonObject>& Params)
{
	const TArray<TSharedPtr<FJsonValue>>* SourcePaths;
	if (!Params->TryGetArrayField(TEXT("source_paths"), SourcePaths) || !SourcePaths || SourcePaths->Num() == 0)
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: source_paths (array of file paths)"));
	}

	FString DestPath = ExtractOptionalString(Params, TEXT("destination_path"), TEXT("/Game/Imported/"));
	bool bReplace = ExtractOptionalBool(Params, TEXT("replace_existing"), true);

	TArray<TSharedPtr<FJsonValue>> Results;
	int32 SuccessCount = 0;
	int32 FailCount = 0;
	TArray<FString> Errors;

	for (const auto& PathVal : *SourcePaths)
	{
		FString SrcPath;
		if (!PathVal->TryGetString(SrcPath))
		{
			continue;
		}

		FString ImportError;
		TSharedPtr<FJsonObject> ImportResult = ImportSingleFile(SrcPath, DestPath, bReplace, ImportError);

		if (ImportResult.IsValid())
		{
			Results.Add(MakeShared<FJsonValueObject>(ImportResult));
			SuccessCount++;
		}
		else
		{
			FailCount++;
			Errors.Add(FString::Printf(TEXT("%s: %s"), *FPaths::GetCleanFilename(SrcPath), *ImportError));
		}
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetNumberField(TEXT("success_count"), SuccessCount);
	ResultData->SetNumberField(TEXT("fail_count"), FailCount);
	ResultData->SetArrayField(TEXT("imported"), Results);
	if (Errors.Num() > 0)
	{
		ResultData->SetArrayField(TEXT("errors"), StringArrayToJsonArray(Errors));
	}

	FMCPToolResult Result = FMCPToolResult::Success(
		FString::Printf(TEXT("Imported %d of %d files"), SuccessCount, SuccessCount + FailCount),
		ResultData);

	if (FailCount > 0)
	{
		Result.Warnings.Add(FString::Printf(TEXT("%d files failed to import"), FailCount));
	}
	return Result;
}

TSharedPtr<FJsonObject> FMCPTool_ImportAsset::ImportSingleFile(const FString& SourcePath, const FString& DestPath,
	bool bReplace, FString& OutError)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.FileExists(*SourcePath))
	{
		OutError = FString::Printf(TEXT("Source file not found: %s"), *SourcePath);
		return nullptr;
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	IAssetTools& AssetTools = AssetToolsModule.Get();

	UAutomatedAssetImportData* ImportData = NewObject<UAutomatedAssetImportData>();
	ImportData->Filenames.Add(SourcePath);
	ImportData->DestinationPath = DestPath;
	ImportData->bReplaceExisting = bReplace;
	ImportData->bSkipReadOnly = true;

	TArray<UObject*> ImportedAssets = AssetTools.ImportAssetsAutomated(ImportData);

	if (ImportedAssets.Num() == 0)
	{
		OutError = FString::Printf(TEXT("Import returned no assets for: %s. Check if a factory exists for this file type."), *SourcePath);
		return nullptr;
	}

	UObject* ImportedAsset = ImportedAssets[0];

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("source_file"), SourcePath);
	Result->SetStringField(TEXT("asset_path"), ImportedAsset->GetPathName());
	Result->SetStringField(TEXT("asset_name"), ImportedAsset->GetName());
	Result->SetStringField(TEXT("asset_class"), ImportedAsset->GetClass()->GetName());
	Result->SetNumberField(TEXT("assets_imported"), ImportedAssets.Num());

	UE_LOG(LogUnrealClaude, Log, TEXT("Imported asset: %s -> %s"), *SourcePath, *ImportedAsset->GetPathName());

	return Result;
}
