// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Read a property value from an actor
 * Complement to set_property — uses the same dot-notation path syntax.
 */
class FMCPTool_GetProperty : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("get_property");
		Info.Description = TEXT(
			"Read any property value from an actor, including component properties.\n\n"
			"Uses the same dot-notation property paths as set_property.\n\n"
			"Property path examples:\n"
			"- 'bHidden' - Actor visibility\n"
			"- 'LightComponent.Intensity' - Light intensity\n"
			"- 'StaticMeshComponent.RelativeScale3D' - Mesh scale\n"
			"- 'RootComponent.RelativeLocation' - Root position\n\n"
			"Returns: The property value as a JSON-compatible type."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("actor_name"), TEXT("string"), TEXT("The name of the actor to read from"), true),
			FMCPToolParameter(TEXT("property"),   TEXT("string"), TEXT("The property path to read (e.g., 'bHidden', 'LightComponent.Intensity')"), true)
		};
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	TSharedPtr<FJsonValue> ReadPropertyToJson(UObject* Object, const FString& PropertyPath, FString& OutError);
	TSharedPtr<FJsonValue> PropertyValueToJson(FProperty* Property, const void* ValuePtr);
};
