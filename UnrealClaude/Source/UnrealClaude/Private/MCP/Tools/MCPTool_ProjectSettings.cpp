// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_ProjectSettings.h"
#include "UnrealClaudeModule.h"

#include "GameMapsSettings.h"
#include "GeneralProjectSettings.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/GameModeBase.h"
#include "Misc/ConfigCacheIni.h"

FMCPToolResult FMCPTool_ProjectSettings::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	if (Operation == TEXT("get_maps_settings"))
	{
		return ExecuteGetMapsSettings(Params);
	}
	if (Operation == TEXT("set_maps_settings"))
	{
		return ExecuteSetMapsSettings(Params);
	}
	if (Operation == TEXT("get_project_info"))
	{
		return ExecuteGetProjectInfo(Params);
	}
	if (Operation == TEXT("get_input_settings"))
	{
		return ExecuteGetInputSettings(Params);
	}
	if (Operation == TEXT("set_input_settings"))
	{
		return ExecuteSetInputSettings(Params);
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: %s. Valid: get_maps_settings, set_maps_settings, get_project_info, get_input_settings, set_input_settings"),
		*Operation));
}

FMCPToolResult FMCPTool_ProjectSettings::ExecuteGetMapsSettings(const TSharedRef<FJsonObject>& Params)
{
	const UGameMapsSettings* MapSettings = GetDefault<UGameMapsSettings>();
	if (!MapSettings)
	{
		return FMCPToolResult::Error(TEXT("Failed to load GameMapsSettings"));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();

	ResultData->SetStringField(TEXT("default_game_mode"), UGameMapsSettings::GetGlobalDefaultGameMode());
	ResultData->SetStringField(TEXT("game_default_map"), UGameMapsSettings::GetGameDefaultMap());
	ResultData->SetStringField(TEXT("server_default_map"), UGameMapsSettings::GetGameDefaultMap(EDefaultMapRequestType::Server));
	ResultData->SetStringField(TEXT("game_instance_class"), MapSettings->GameInstanceClass.ToString());

	return FMCPToolResult::Success(TEXT("Retrieved maps/modes settings"), ResultData);
}

FMCPToolResult FMCPTool_ProjectSettings::ExecuteSetMapsSettings(const TSharedRef<FJsonObject>& Params)
{
	UGameMapsSettings* MapSettings = GetMutableDefault<UGameMapsSettings>();
	if (!MapSettings)
	{
		return FMCPToolResult::Error(TEXT("Failed to load GameMapsSettings"));
	}

	TArray<FString> ChangedFields;

	FString DefaultGameMode = ExtractOptionalString(Params, TEXT("default_game_mode"));
	if (!DefaultGameMode.IsEmpty())
	{
		UGameMapsSettings::SetGlobalDefaultGameMode(DefaultGameMode);
		ChangedFields.Add(FString::Printf(TEXT("default_game_mode=%s"), *DefaultGameMode));
	}

	FString GameDefaultMap = ExtractOptionalString(Params, TEXT("game_default_map"));
	if (!GameDefaultMap.IsEmpty())
	{
		UGameMapsSettings::SetGameDefaultMap(GameDefaultMap);
		ChangedFields.Add(FString::Printf(TEXT("game_default_map=%s"), *GameDefaultMap));
	}

	if (ChangedFields.Num() == 0)
	{
		return FMCPToolResult::Error(TEXT("No properties specified. Provide: default_game_mode, game_default_map, server_default_map, or game_instance_class"));
	}

	MapSettings->SaveConfig();

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetArrayField(TEXT("changed"), StringArrayToJsonArray(ChangedFields));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Updated %d maps settings"), ChangedFields.Num()),
		ResultData);
}

FMCPToolResult FMCPTool_ProjectSettings::ExecuteGetProjectInfo(const TSharedRef<FJsonObject>& Params)
{
	const UGeneralProjectSettings* ProjSettings = GetDefault<UGeneralProjectSettings>();
	if (!ProjSettings)
	{
		return FMCPToolResult::Error(TEXT("Failed to load GeneralProjectSettings"));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("project_name"), ProjSettings->ProjectName);
	ResultData->SetStringField(TEXT("company_name"), ProjSettings->CompanyName);
	ResultData->SetStringField(TEXT("project_version"), ProjSettings->ProjectVersion);
	ResultData->SetStringField(TEXT("description"), ProjSettings->Description);
	ResultData->SetStringField(TEXT("homepage"), ProjSettings->Homepage);
	ResultData->SetStringField(TEXT("project_id"), ProjSettings->ProjectID.ToString());

	FString ProjectDir = FPaths::GetProjectFilePath();
	ResultData->SetStringField(TEXT("project_file"), ProjectDir);
	ResultData->SetStringField(TEXT("project_directory"), FPaths::ProjectDir());
	ResultData->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());

	return FMCPToolResult::Success(TEXT("Retrieved project info"), ResultData);
}

FMCPToolResult FMCPTool_ProjectSettings::ExecuteGetInputSettings(const TSharedRef<FJsonObject>& Params)
{
	const UInputSettings* InputSettings = GetDefault<UInputSettings>();
	if (!InputSettings)
	{
		return FMCPToolResult::Error(TEXT("Failed to load InputSettings"));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();

	ResultData->SetStringField(TEXT("default_touch_interface"),
		InputSettings->DefaultTouchInterface.ToString());
	ResultData->SetBoolField(TEXT("use_mouse_for_touch"), InputSettings->bUseMouseForTouch);
	ResultData->SetBoolField(TEXT("enable_mouse_smoothing"), InputSettings->bEnableMouseSmoothing);
	ResultData->SetBoolField(TEXT("enable_fov_scaling"), InputSettings->bEnableFOVScaling);
	ResultData->SetBoolField(TEXT("always_show_touch_interface"), InputSettings->bAlwaysShowTouchInterface);

	TArray<TSharedPtr<FJsonValue>> AxisConfigs;
	for (const FInputAxisConfigEntry& Entry : InputSettings->AxisConfig)
	{
		TSharedPtr<FJsonObject> AxisObj = MakeShared<FJsonObject>();
		AxisObj->SetStringField(TEXT("key"), Entry.AxisKeyName.ToString());
		TSharedPtr<FJsonObject> PropsObj = MakeShared<FJsonObject>();
		PropsObj->SetNumberField(TEXT("dead_zone"), Entry.AxisProperties.DeadZone);
		PropsObj->SetNumberField(TEXT("sensitivity"), Entry.AxisProperties.Sensitivity);
		PropsObj->SetBoolField(TEXT("invert"), Entry.AxisProperties.bInvert);
		AxisObj->SetObjectField(TEXT("properties"), PropsObj);
		AxisConfigs.Add(MakeShared<FJsonValueObject>(AxisObj));
	}
	ResultData->SetArrayField(TEXT("axis_config"), AxisConfigs);

	return FMCPToolResult::Success(TEXT("Retrieved input settings"), ResultData);
}

FMCPToolResult FMCPTool_ProjectSettings::ExecuteSetInputSettings(const TSharedRef<FJsonObject>& Params)
{
	UInputSettings* InputSettings = GetMutableDefault<UInputSettings>();
	if (!InputSettings)
	{
		return FMCPToolResult::Error(TEXT("Failed to load InputSettings"));
	}

	TArray<FString> ChangedFields;

	FString TouchInterface = ExtractOptionalString(Params, TEXT("default_touch_interface"));
	if (!TouchInterface.IsEmpty())
	{
		InputSettings->DefaultTouchInterface = FSoftObjectPath(TouchInterface);
		ChangedFields.Add(TEXT("default_touch_interface"));
	}

	// DefaultInputComponentClass and DefaultPlayerInputClass are private in UE 5.5
	// Use DefaultTouchInterface for now; other input settings require config file editing

	if (ChangedFields.Num() == 0)
	{
		return FMCPToolResult::Error(TEXT("No properties specified to set"));
	}

	InputSettings->SaveConfig();

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetArrayField(TEXT("changed"), StringArrayToJsonArray(ChangedFields));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Updated %d input settings"), ChangedFields.Num()),
		ResultData);
}
