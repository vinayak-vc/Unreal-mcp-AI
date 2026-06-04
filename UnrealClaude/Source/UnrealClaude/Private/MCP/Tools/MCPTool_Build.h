// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Build / Compile
 *
 * Trigger project compilation and hot reload from MCP.
 *
 * Operations:
 *   - hot_reload: Trigger Live Coding recompile (fastest, Win64 only)
 *   - compile_blueprint: Force-compile a specific Blueprint asset
 */
class FMCPTool_Build : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("build");
		Info.Description = TEXT(
			"Trigger compilation and hot reload.\n\n"
			"Operations:\n"
			"- 'hot_reload': Trigger Live Coding recompile (Win64 editor only, fastest iteration)\n"
			"- 'compile_blueprint': Force-compile a specific Blueprint asset\n"
			"- 'compile_all_blueprints': Compile all dirty Blueprints in the project\n\n"
			"hot_reload compiles only changed C++ files and patches them into the running editor.\n"
			"compile_blueprint uses FKismetEditorUtilities to recompile a Blueprint and reports errors."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation: 'hot_reload', 'compile_blueprint', 'compile_all_blueprints'"), true),
			FMCPToolParameter(TEXT("blueprint_path"), TEXT("string"),
				TEXT("Path to Blueprint asset (for compile_blueprint)"), false),
			FMCPToolParameter(TEXT("save_after_compile"), TEXT("boolean"),
				TEXT("Save Blueprint after successful compile (default: true)"), false, TEXT("true"))
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	FMCPToolResult ExecuteHotReload(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteCompileBlueprint(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteCompileAllBlueprints(const TSharedRef<FJsonObject>& Params);
};
