// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_Build.h"
#include "UnrealClaudeModule.h"
#include "MCP/MCPParamValidator.h"

#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"

#if PLATFORM_WINDOWS
#include "ILiveCodingModule.h"
#endif

FMCPToolResult FMCPTool_Build::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	if (Operation == TEXT("hot_reload"))
	{
		return ExecuteHotReload(Params);
	}
	if (Operation == TEXT("compile_blueprint"))
	{
		return ExecuteCompileBlueprint(Params);
	}
	if (Operation == TEXT("compile_all_blueprints"))
	{
		return ExecuteCompileAllBlueprints(Params);
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: %s. Valid: hot_reload, compile_blueprint, compile_all_blueprints"),
		*Operation));
}

FMCPToolResult FMCPTool_Build::ExecuteHotReload(const TSharedRef<FJsonObject>& Params)
{
#if PLATFORM_WINDOWS
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
	if (!LiveCoding)
	{
		return FMCPToolResult::Error(TEXT("Live Coding module not available. Ensure Live Coding is enabled in Editor Preferences."));
	}

	if (!LiveCoding->IsEnabledForSession())
	{
		return FMCPToolResult::Error(TEXT("Live Coding is not enabled for this session. Enable it in Editor Preferences > Live Coding."));
	}

	if (LiveCoding->IsCompiling())
	{
		return FMCPToolResult::Error(TEXT("Live Coding compilation already in progress"));
	}

	LiveCoding->EnableForSession(true);
	LiveCoding->Compile();

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetBoolField(TEXT("compile_triggered"), true);
	ResultData->SetStringField(TEXT("method"), TEXT("LiveCoding"));

	return FMCPToolResult::Success(
		TEXT("Live Coding compile triggered. Check Output Log for results."),
		ResultData);
#else
	return FMCPToolResult::Error(TEXT("Hot reload via Live Coding is only available on Windows"));
#endif
}

FMCPToolResult FMCPTool_Build::ExecuteCompileBlueprint(const TSharedRef<FJsonObject>& Params)
{
	FString BlueprintPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractAndValidate(Params, TEXT("blueprint_path"), FMCPParamValidator::ValidateBlueprintPath, BlueprintPath, Error))
	{
		return Error.GetValue();
	}

	UObject* Asset = LoadObject<UObject>(nullptr, *BlueprintPath);
	if (!Asset)
	{
		FString AdjustedPath = BlueprintPath;
		if (!AdjustedPath.Contains(TEXT(".")))
		{
			AdjustedPath += TEXT(".") + FPaths::GetBaseFilename(BlueprintPath);
		}
		Asset = LoadObject<UObject>(nullptr, *AdjustedPath);
	}

	UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
	if (!Blueprint)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None);

	bool bSave = ExtractOptionalBool(Params, TEXT("save_after_compile"), true);
	bool bSaved = false;

	if (bSave && Blueprint->Status != BS_Error)
	{
		FString PackagePath = Blueprint->GetOutermost()->GetPathName();
		bSaved = UEditorAssetLibrary::SaveAsset(PackagePath, false);
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("blueprint_path"), Blueprint->GetPathName());
	ResultData->SetStringField(TEXT("blueprint_name"), Blueprint->GetName());

	FString StatusStr;
	switch (Blueprint->Status)
	{
	case BS_Unknown: StatusStr = TEXT("Unknown"); break;
	case BS_Dirty: StatusStr = TEXT("Dirty"); break;
	case BS_Error: StatusStr = TEXT("Error"); break;
	case BS_UpToDate: StatusStr = TEXT("UpToDate"); break;
	case BS_BeingCreated: StatusStr = TEXT("BeingCreated"); break;
	case BS_UpToDateWithWarnings: StatusStr = TEXT("UpToDateWithWarnings"); break;
	default: StatusStr = TEXT("Unknown"); break;
	}
	ResultData->SetStringField(TEXT("status"), StatusStr);
	ResultData->SetBoolField(TEXT("saved"), bSaved);

	if (Blueprint->Status == BS_Error)
	{
		return FMCPToolResult::Error(
			FString::Printf(TEXT("Blueprint '%s' compiled with errors. Check Output Log."), *Blueprint->GetName()));
	}

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Blueprint '%s' compiled successfully (status: %s)"), *Blueprint->GetName(), *StatusStr),
		ResultData);
}

FMCPToolResult FMCPTool_Build::ExecuteCompileAllBlueprints(const TSharedRef<FJsonObject>& Params)
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	TArray<FAssetData> BlueprintAssets;
	AssetRegistry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), BlueprintAssets, true);

	int32 CompiledCount = 0;
	int32 ErrorCount = 0;
	int32 SkippedCount = 0;
	TArray<FString> ErrorBlueprints;

	bool bSave = ExtractOptionalBool(Params, TEXT("save_after_compile"), true);

	for (const FAssetData& AssetData : BlueprintAssets)
	{
		if (!AssetData.PackageName.ToString().StartsWith(TEXT("/Game/")))
		{
			SkippedCount++;
			continue;
		}

		UBlueprint* BP = Cast<UBlueprint>(AssetData.GetAsset());
		if (!BP)
		{
			SkippedCount++;
			continue;
		}

		if (BP->Status == BS_UpToDate)
		{
			SkippedCount++;
			continue;
		}

		FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None);
		CompiledCount++;

		if (BP->Status == BS_Error)
		{
			ErrorCount++;
			ErrorBlueprints.Add(BP->GetName());
		}
		else if (bSave)
		{
			UEditorAssetLibrary::SaveAsset(BP->GetOutermost()->GetPathName(), false);
		}
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetNumberField(TEXT("total_found"), BlueprintAssets.Num());
	ResultData->SetNumberField(TEXT("compiled"), CompiledCount);
	ResultData->SetNumberField(TEXT("errors"), ErrorCount);
	ResultData->SetNumberField(TEXT("skipped"), SkippedCount);
	if (ErrorBlueprints.Num() > 0)
	{
		ResultData->SetArrayField(TEXT("error_blueprints"), StringArrayToJsonArray(ErrorBlueprints));
	}

	FString Msg = FString::Printf(TEXT("Compiled %d Blueprints (%d errors, %d skipped)"),
		CompiledCount, ErrorCount, SkippedCount);

	if (ErrorCount > 0)
	{
		FMCPToolResult Result = FMCPToolResult::Success(Msg, ResultData);
		Result.Warnings.Add(FString::Printf(TEXT("%d Blueprints had compile errors"), ErrorCount));
		return Result;
	}

	return FMCPToolResult::Success(Msg, ResultData);
}
