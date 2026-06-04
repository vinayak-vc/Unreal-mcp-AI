// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Curve asset operations
 *
 * Create and edit CurveFloat, CurveVector, and CurveLinearColor assets.
 *
 * Operations:
 *   - create: Create a new curve asset
 *   - add_key: Add a key to a curve
 *   - get_keys: Read all keys from a curve
 *   - evaluate: Evaluate curve at a specific time
 *   - set_keys: Replace all keys on a curve
 */
class FMCPTool_Curve : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("curve");
		Info.Description = TEXT(
			"Create and edit Curve assets (CurveFloat, CurveVector, CurveLinearColor).\n\n"
			"Operations:\n"
			"- 'create': Create new curve asset\n"
			"- 'add_key': Add a key at time with value\n"
			"- 'get_keys': Read all keys from a curve\n"
			"- 'evaluate': Get curve value at a specific time\n"
			"- 'set_keys': Replace all keys (bulk set)\n\n"
			"Curve types: 'Float', 'Vector', 'LinearColor'\n\n"
			"Key format for Float: {time, value}\n"
			"Key format for Vector: {time, x, y, z}\n"
			"Key format for LinearColor: {time, r, g, b, a}"
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation to perform"), true),
			FMCPToolParameter(TEXT("curve_path"), TEXT("string"),
				TEXT("Path to existing curve asset"), false),
			FMCPToolParameter(TEXT("package_path"), TEXT("string"),
				TEXT("Package path for new curve (default: '/Game/Curves/')"), false, TEXT("/Game/Curves/")),
			FMCPToolParameter(TEXT("curve_name"), TEXT("string"),
				TEXT("Name for new curve (for create)"), false),
			FMCPToolParameter(TEXT("curve_type"), TEXT("string"),
				TEXT("Type: 'Float', 'Vector', 'LinearColor' (default: 'Float')"), false, TEXT("Float")),
			FMCPToolParameter(TEXT("time"), TEXT("number"),
				TEXT("Key time position (for add_key/evaluate)"), false),
			FMCPToolParameter(TEXT("value"), TEXT("number"),
				TEXT("Float value (for add_key with Float curve)"), false),
			FMCPToolParameter(TEXT("vector_value"), TEXT("object"),
				TEXT("{x, y, z} value (for Vector curve)"), false),
			FMCPToolParameter(TEXT("color_value"), TEXT("object"),
				TEXT("{r, g, b, a} value (for LinearColor curve)"), false),
			FMCPToolParameter(TEXT("keys"), TEXT("array"),
				TEXT("Array of key objects for set_keys"), false),
			FMCPToolParameter(TEXT("interp_mode"), TEXT("string"),
				TEXT("Interpolation: 'Linear', 'Constant', 'Cubic' (default: 'Cubic')"), false, TEXT("Cubic"))
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	FMCPToolResult ExecuteCreate(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteAddKey(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteGetKeys(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteEvaluate(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteSetKeys(const TSharedRef<FJsonObject>& Params);
};
