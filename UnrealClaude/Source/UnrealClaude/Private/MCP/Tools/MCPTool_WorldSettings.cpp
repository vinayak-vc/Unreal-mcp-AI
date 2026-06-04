// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_WorldSettings.h"
#include "UnrealClaudeModule.h"

#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "Editor.h"

FMCPToolResult FMCPTool_WorldSettings::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	if (Operation == TEXT("get"))
	{
		return ExecuteGet(Params);
	}
	if (Operation == TEXT("set"))
	{
		return ExecuteSet(Params);
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: %s. Valid: get, set"), *Operation));
}

FMCPToolResult FMCPTool_WorldSettings::ExecuteGet(const TSharedRef<FJsonObject>& Params)
{
	UWorld* World = nullptr;
	if (auto Err = ValidateEditorContext(World))
	{
		return Err.GetValue();
	}

	AWorldSettings* Settings = World->GetWorldSettings();
	if (!Settings)
	{
		return FMCPToolResult::Error(TEXT("No WorldSettings found in current level"));
	}

	TSharedPtr<FJsonObject> ResultData = WorldSettingsToJson(Settings);
	return FMCPToolResult::Success(TEXT("Retrieved world settings"), ResultData);
}

FMCPToolResult FMCPTool_WorldSettings::ExecuteSet(const TSharedRef<FJsonObject>& Params)
{
	UWorld* World = nullptr;
	if (auto Err = ValidateEditorContext(World))
	{
		return Err.GetValue();
	}

	AWorldSettings* Settings = World->GetWorldSettings();
	if (!Settings)
	{
		return FMCPToolResult::Error(TEXT("No WorldSettings found in current level"));
	}

	TArray<FString> ChangedFields;

	// GameMode override
	FString GameModeClass = ExtractOptionalString(Params, TEXT("game_mode_class"));
	if (GameModeClass.IsEmpty())
	{
		GameModeClass = ExtractOptionalString(Params, TEXT("default_game_mode"));
	}
	if (!GameModeClass.IsEmpty())
	{
		if (GameModeClass == TEXT("None") || GameModeClass == TEXT("none") || GameModeClass == TEXT("null"))
		{
			Settings->DefaultGameMode = nullptr;
			ChangedFields.Add(TEXT("game_mode_class=None"));
		}
		else
		{
			UClass* GMClass = LoadClass<AGameModeBase>(nullptr, *GameModeClass);
			if (!GMClass && !GameModeClass.EndsWith(TEXT("_C")))
			{
				GMClass = LoadClass<AGameModeBase>(nullptr, *(GameModeClass + TEXT("_C")));
			}
			if (!GMClass)
			{
				GMClass = LoadClass<AGameModeBase>(nullptr,
					*FString::Printf(TEXT("/Script/Engine.%s"), *GameModeClass));
			}
			if (!GMClass)
			{
				return FMCPToolResult::Error(FString::Printf(
					TEXT("GameMode class not found: %s"), *GameModeClass));
			}
			Settings->DefaultGameMode = GMClass;
			ChangedFields.Add(FString::Printf(TEXT("game_mode_class=%s"), *GMClass->GetPathName()));
		}
	}

	double GravityZ;
	if (Params->TryGetNumberField(TEXT("gravity_z"), GravityZ))
	{
		Settings->bGlobalGravitySet = true;
		Settings->GlobalGravityZ = static_cast<float>(GravityZ);
		ChangedFields.Add(FString::Printf(TEXT("gravity_z=%.1f"), GravityZ));
	}

	double KillZ;
	if (Params->TryGetNumberField(TEXT("kill_z"), KillZ))
	{
		Settings->KillZ = static_cast<float>(KillZ);
		ChangedFields.Add(FString::Printf(TEXT("kill_z=%.1f"), KillZ));
	}

	double WorldToMeters;
	if (Params->TryGetNumberField(TEXT("world_to_meters"), WorldToMeters))
	{
		Settings->WorldToMeters = static_cast<float>(WorldToMeters);
		ChangedFields.Add(FString::Printf(TEXT("world_to_meters=%.1f"), WorldToMeters));
	}

	bool bEnableWorldBounds;
	if (Params->TryGetBoolField(TEXT("enable_world_bounds"), bEnableWorldBounds))
	{
		Settings->bEnableWorldBoundsChecks = bEnableWorldBounds;
		ChangedFields.Add(FString::Printf(TEXT("enable_world_bounds=%s"), bEnableWorldBounds ? TEXT("true") : TEXT("false")));
	}

	// Navigation system enable/disable uses NavigationSystemConfig in UE 5.5+
	// Setting it requires replacing the config object, which is complex — skipping for now

	if (ChangedFields.Num() == 0)
	{
		return FMCPToolResult::Error(TEXT("No properties specified to set. Provide at least one: game_mode_class, gravity_z, kill_z, world_to_meters, enable_world_bounds, enable_navigation"));
	}

	MarkActorDirty(Settings);
	MarkWorldDirty(World);

	TSharedPtr<FJsonObject> ResultData = WorldSettingsToJson(Settings);
	ResultData->SetArrayField(TEXT("changed"), StringArrayToJsonArray(ChangedFields));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Updated %d world settings: %s"), ChangedFields.Num(), *FString::Join(ChangedFields, TEXT(", "))),
		ResultData);
}

TSharedPtr<FJsonObject> FMCPTool_WorldSettings::WorldSettingsToJson(AWorldSettings* Settings)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!Settings)
	{
		return Json;
	}

	if (Settings->DefaultGameMode)
	{
		Json->SetStringField(TEXT("game_mode_class"), Settings->DefaultGameMode->GetPathName());
		Json->SetStringField(TEXT("game_mode_name"), Settings->DefaultGameMode->GetName());
	}
	else
	{
		Json->SetStringField(TEXT("game_mode_class"), TEXT("None"));
	}

	Json->SetBoolField(TEXT("global_gravity_set"), Settings->bGlobalGravitySet);
	Json->SetNumberField(TEXT("gravity_z"), Settings->bGlobalGravitySet ? Settings->GlobalGravityZ : -980.0f);
	Json->SetNumberField(TEXT("kill_z"), Settings->KillZ);
	Json->SetNumberField(TEXT("world_to_meters"), Settings->WorldToMeters);
	Json->SetBoolField(TEXT("enable_world_bounds"), Settings->bEnableWorldBoundsChecks);
	Json->SetBoolField(TEXT("has_navigation_config"), Settings->GetNavigationSystemConfig() != nullptr);

	Json->SetStringField(TEXT("level_name"), Settings->GetLevel() ? Settings->GetLevel()->GetOuter()->GetName() : TEXT("Unknown"));

	return Json;
}
