// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Level / editor viewport operations
 *
 * Operations:
 *   - save            : Save the current level
 *   - get_actor_bounds: Bounding box of an actor (origin + extent)
 *   - select_actors   : Select actors in the editor viewport by name/class
 *   - focus_viewport  : Move editor camera to focus on an actor
 */
class FMCPTool_Level : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("level");
		Info.Description = TEXT(
			"Level and editor viewport operations.\n\n"
			"Operations:\n"
			"- 'save': Save the current level to disk\n"
			"- 'get_actor_bounds': Get bounding box (origin + half-extent) of an actor\n"
			"- 'select_actors': Select actors in editor viewport (by name or class)\n"
			"- 'focus_viewport': Move editor camera to focus on actor\n\n"
			"Useful for: verifying actor placement, centering particles, saving after edits."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"),    TEXT("string"), TEXT("Operation: save, get_actor_bounds, select_actors, focus_viewport"), true),
			FMCPToolParameter(TEXT("actor_name"),   TEXT("string"), TEXT("Actor name/label (required for bounds/focus/select)"), false),
			FMCPToolParameter(TEXT("class_filter"), TEXT("string"), TEXT("Class name filter for select_actors"), false),
			FMCPToolParameter(TEXT("name_filter"),  TEXT("string"), TEXT("Name substring filter for select_actors"), false),
			FMCPToolParameter(TEXT("add_to_selection"), TEXT("boolean"), TEXT("Append to current selection (default false)"), false)
		};
		Info.Annotations = FMCPToolAnnotations::ReadOnly(); // save marks dirty but doesn't destroy
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	FMCPToolResult ExecuteSave(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteGetActorBounds(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteSelectActors(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteFocusViewport(const TSharedRef<FJsonObject>& Params);
};
