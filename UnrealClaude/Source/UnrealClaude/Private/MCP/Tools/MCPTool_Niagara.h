// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Niagara particle system operations
 *
 * Operations:
 *   - spawn_system  : Spawn a NiagaraSystem asset at a world location
 *   - set_parameter : Set a float/vector/bool/int parameter on a spawned NiagaraComponent
 *   - list_systems  : Find NiagaraSystem assets in the project
 */
class FMCPTool_Niagara : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("niagara");
		Info.Description = TEXT(
			"Niagara particle system operations.\n\n"
			"Operations:\n"
			"- 'spawn_system': Spawn a NiagaraSystem at world location\n"
			"- 'set_parameter': Set a parameter on a spawned NiagaraComponent actor\n"
			"- 'list_systems': Find NiagaraSystem assets in the project\n\n"
			"Example spawn_system params:\n"
			"  system_path: '/Game/VFX/NS_MyEffect'\n"
			"  location: {x:0, y:0, z:100}\n"
			"  rotation: {pitch:0, yaw:0, roll:0}\n"
			"  scale: {x:1, y:1, z:1}\n"
			"  auto_destroy: true\n"
			"  actor_name: 'MyParticles'  (optional label)\n\n"
			"Returns: Spawned actor info or list of assets."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"),   TEXT("string"), TEXT("Operation: spawn_system, set_parameter, list_systems"), true),
			FMCPToolParameter(TEXT("system_path"), TEXT("string"), TEXT("Asset path to NiagaraSystem (e.g. '/Game/VFX/NS_Burst')"), false),
			FMCPToolParameter(TEXT("location"),    TEXT("object"), TEXT("{x, y, z} world location"), false),
			FMCPToolParameter(TEXT("rotation"),    TEXT("object"), TEXT("{pitch, yaw, roll} rotation"), false),
			FMCPToolParameter(TEXT("scale"),       TEXT("object"), TEXT("{x, y, z} scale (default 1,1,1)"), false),
			FMCPToolParameter(TEXT("auto_destroy"),TEXT("boolean"),TEXT("Destroy after effect completes (default true)"), false),
			FMCPToolParameter(TEXT("actor_name"),  TEXT("string"), TEXT("Optional label for spawned actor"), false),
			FMCPToolParameter(TEXT("param_name"),  TEXT("string"), TEXT("Niagara parameter name for set_parameter"), false),
			FMCPToolParameter(TEXT("param_type"),  TEXT("string"), TEXT("float | vector | bool | int | color"), false),
			FMCPToolParameter(TEXT("param_value"), TEXT("any"),    TEXT("Parameter value"), false),
			FMCPToolParameter(TEXT("package_path"),TEXT("string"), TEXT("Search path for list_systems (default /Game/)"), false),
			FMCPToolParameter(TEXT("name_filter"), TEXT("string"), TEXT("Name substring filter for list_systems"), false),
			FMCPToolParameter(TEXT("limit"),       TEXT("number"), TEXT("Max results for list_systems (default 50)"), false)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	FMCPToolResult ExecuteSpawnSystem(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteSetParameter(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteListSystems(const TSharedRef<FJsonObject>& Params);
};
