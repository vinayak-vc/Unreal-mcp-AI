// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Set Class Defaults (Blueprint CDO)
 *
 * Batch get/set default property values on Blueprint Class Default Objects.
 * Works with both Blueprint-declared variables and C++ UPROPERTY(EditDefaultsOnly).
 *
 * Operations:
 *   - get_defaults: Read current CDO property values
 *   - set_defaults: Batch set multiple CDO properties in one call
 */
class FMCPTool_SetClassDefaults : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("class_defaults");
		Info.Description = TEXT(
			"Batch get/set Blueprint Class Default Object (CDO) properties.\n\n"
			"Operations:\n"
			"- 'get_defaults': Read CDO property values. Returns all editable defaults or specific ones.\n"
			"- 'set_defaults': Batch set multiple CDO properties in one call.\n\n"
			"Works with Blueprint variables AND C++ UPROPERTY(EditDefaultsOnly) properties.\n\n"
			"The 'properties' parameter is a JSON object where keys are property names and values are the values to set.\n"
			"Example: {\"MaxHealth\": 100, \"PlayerName\": \"Hero\", \"bCanFly\": true}\n\n"
			"Supports: bool, int, float, string, name, text, vector, rotator, color, enum, "
			"soft object references, and class references."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation: 'get_defaults' or 'set_defaults'"), true),
			FMCPToolParameter(TEXT("blueprint_path"), TEXT("string"),
				TEXT("Path to Blueprint asset (e.g., '/Game/Blueprints/BP_MyActor')"), true),
			FMCPToolParameter(TEXT("properties"), TEXT("object"),
				TEXT("JSON object of property names to values (for set_defaults)"), false),
			FMCPToolParameter(TEXT("property_names"), TEXT("array"),
				TEXT("Array of property names to read (for get_defaults; omit for all)"), false),
			FMCPToolParameter(TEXT("include_inherited"), TEXT("boolean"),
				TEXT("Include inherited properties in get_defaults (default: false)"), false, TEXT("false"))
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	FMCPToolResult ExecuteGetDefaults(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteSetDefaults(const TSharedRef<FJsonObject>& Params);

	UBlueprint* LoadBlueprint(const FString& Path, FString& OutError);
	bool SetPropertyValue(UObject* CDO, FProperty* Prop, const TSharedPtr<FJsonValue>& Value, FString& OutError);
	TSharedPtr<FJsonValue> GetPropertyValue(UObject* CDO, FProperty* Prop);
};
