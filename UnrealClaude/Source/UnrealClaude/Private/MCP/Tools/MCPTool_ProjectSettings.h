// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Project Settings
 *
 * Read and write project-level settings — default maps, game mode, input config, etc.
 *
 * Operations:
 *   - get_maps_settings: Return GameMapsSettings (default maps, game modes)
 *   - set_maps_settings: Modify GameMapsSettings
 *   - get_project_info: Return general project settings
 *   - get_input_settings: Return input-related project settings
 *   - set_input_settings: Modify input-related project settings
 */
class FMCPTool_ProjectSettings : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("project_settings");
		Info.Description = TEXT(
			"Read and write Unreal project settings.\n\n"
			"Operations:\n"
			"- 'get_maps_settings': Return default maps, game instances, game modes\n"
			"- 'set_maps_settings': Set default game mode, default maps, etc.\n"
			"- 'get_project_info': Return general project info (name, version, description)\n"
			"- 'get_input_settings': Return input-related settings\n"
			"- 'set_input_settings': Modify input settings\n\n"
			"Settable maps properties:\n"
			"- default_game_mode: Default GameMode class path\n"
			"- game_default_map: Editor startup map\n"
			"- server_default_map: Server default map\n"
			"- game_instance_class: GameInstance class path\n\n"
			"Settable input properties:\n"
			"- default_touch_interface: Touch interface setup asset path\n"
			"- default_input_component_class: Default input component class\n"
			"- default_player_input_class: Default player input class"
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation to perform (see description)"), true),
			FMCPToolParameter(TEXT("default_game_mode"), TEXT("string"),
				TEXT("Default GameMode class path"), false),
			FMCPToolParameter(TEXT("game_default_map"), TEXT("string"),
				TEXT("Editor startup / game default map path"), false),
			FMCPToolParameter(TEXT("server_default_map"), TEXT("string"),
				TEXT("Server default map path"), false),
			FMCPToolParameter(TEXT("game_instance_class"), TEXT("string"),
				TEXT("GameInstance class path"), false),
			FMCPToolParameter(TEXT("default_touch_interface"), TEXT("string"),
				TEXT("Touch interface setup asset path (empty to disable)"), false),
			FMCPToolParameter(TEXT("default_input_component_class"), TEXT("string"),
				TEXT("Default input component class path"), false),
			FMCPToolParameter(TEXT("default_player_input_class"), TEXT("string"),
				TEXT("Default player input class path"), false)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	FMCPToolResult ExecuteGetMapsSettings(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteSetMapsSettings(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteGetProjectInfo(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteGetInputSettings(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteSetInputSettings(const TSharedRef<FJsonObject>& Params);
};
