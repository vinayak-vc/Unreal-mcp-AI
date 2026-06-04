// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Actor Search
 *
 * Advanced actor search with filtering by class, tag, name pattern, property value,
 * component type, and spatial queries.
 *
 * More powerful than get_level_actors — supports tag filtering, property matching,
 * component queries, and radius searches.
 */
class FMCPTool_ActorSearch : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("actor_search");
		Info.Description = TEXT(
			"Advanced actor search with multiple filter criteria.\n\n"
			"All filters are combined with AND logic. At least one filter is required.\n\n"
			"Filter types:\n"
			"- class_name: Exact class match or parent class match (e.g., 'StaticMeshActor')\n"
			"- name_pattern: Substring match on actor name or label (case-insensitive)\n"
			"- tag: Actors with this gameplay tag (exact match)\n"
			"- tags: Array of tags — actor must have ALL of them\n"
			"- has_component: Actors with a component of this class (e.g., 'StaticMeshComponent')\n"
			"- property_name + property_value: Actors where a top-level property equals value\n"
			"- near_location + radius: Spatial search (actors within radius of point)\n\n"
			"Returns: actor name, label, class, location, and matched criteria."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("class_name"), TEXT("string"),
				TEXT("Filter by class name (supports parent class matching)"), false),
			FMCPToolParameter(TEXT("name_pattern"), TEXT("string"),
				TEXT("Substring match on actor name or label"), false),
			FMCPToolParameter(TEXT("tag"), TEXT("string"),
				TEXT("Filter by single actor tag"), false),
			FMCPToolParameter(TEXT("tags"), TEXT("array"),
				TEXT("Filter by multiple tags (actor must have ALL)"), false),
			FMCPToolParameter(TEXT("has_component"), TEXT("string"),
				TEXT("Filter by component class name"), false),
			FMCPToolParameter(TEXT("property_name"), TEXT("string"),
				TEXT("Property name for value matching"), false),
			FMCPToolParameter(TEXT("property_value"), TEXT("string"),
				TEXT("Expected property value (string comparison)"), false),
			FMCPToolParameter(TEXT("near_location"), TEXT("object"),
				TEXT("Center point for spatial search {x, y, z}"), false),
			FMCPToolParameter(TEXT("radius"), TEXT("number"),
				TEXT("Search radius in units (for near_location)"), false, TEXT("1000")),
			FMCPToolParameter(TEXT("limit"), TEXT("number"),
				TEXT("Max results (1-500, default: 50)"), false, TEXT("50")),
			FMCPToolParameter(TEXT("include_hidden"), TEXT("boolean"),
				TEXT("Include hidden actors (default: false)"), false, TEXT("false"))
		};
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
