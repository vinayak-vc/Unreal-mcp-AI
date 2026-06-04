// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: World Settings
 *
 * Get and set level World Settings — GameMode override, gravity, kill Z, etc.
 *
 * Operations:
 *   - get: Return current world settings
 *   - set: Modify world settings properties
 */
class FMCPTool_WorldSettings : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("world_settings");
		Info.Description = TEXT(
			"Get and set level World Settings.\n\n"
			"Operations:\n"
			"- 'get': Return current world settings (GameMode, gravity, kill Z, etc.)\n"
			"- 'set': Modify world settings properties\n\n"
			"Settable properties:\n"
			"- game_mode_class: GameMode override class path (e.g., '/Game/Blueprints/BP_MyGameMode.BP_MyGameMode_C')\n"
			"- gravity_z: World gravity Z value (default: -980)\n"
			"- kill_z: Z value below which actors are killed (default: -10000)\n"
			"- world_to_meters: World scale (default: 100)\n"
			"- enable_world_bounds: Enable world bounds checks\n"
			"- enable_navigation: Enable navigation system\n"
			"- default_game_mode: Same as game_mode_class (alias)\n\n"
			"Note: game_mode_class accepts both Blueprint path (/Game/...) and C++ class name (e.g., 'GameModeBase')"
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation: 'get' or 'set'"), true),
			FMCPToolParameter(TEXT("game_mode_class"), TEXT("string"),
				TEXT("GameMode override class path or C++ class name"), false),
			FMCPToolParameter(TEXT("gravity_z"), TEXT("number"),
				TEXT("World gravity Z value (negative = down, default: -980)"), false),
			FMCPToolParameter(TEXT("kill_z"), TEXT("number"),
				TEXT("Z value below which actors are destroyed"), false),
			FMCPToolParameter(TEXT("world_to_meters"), TEXT("number"),
				TEXT("World-to-meters scale factor"), false),
			FMCPToolParameter(TEXT("enable_world_bounds"), TEXT("boolean"),
				TEXT("Enable world bounds checking"), false),
			FMCPToolParameter(TEXT("enable_navigation"), TEXT("boolean"),
				TEXT("Enable navigation system"), false)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	FMCPToolResult ExecuteGet(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteSet(const TSharedRef<FJsonObject>& Params);

	TSharedPtr<FJsonObject> WorldSettingsToJson(class AWorldSettings* Settings);
};
