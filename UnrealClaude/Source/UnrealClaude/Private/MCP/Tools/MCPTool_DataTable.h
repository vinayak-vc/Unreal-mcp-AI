// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: DataTable operations
 *
 * Create, read, and modify DataTable assets and their rows.
 *
 * Operations:
 *   - create: Create a new DataTable asset with a row struct
 *   - get_rows: Read all or specific rows from a DataTable
 *   - add_row: Add a new row to a DataTable
 *   - modify_row: Modify an existing row
 *   - delete_row: Remove a row from a DataTable
 *   - query: Get DataTable metadata (row struct, row count, row names)
 */
class FMCPTool_DataTable : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("data_table");
		Info.Description = TEXT(
			"Create and modify DataTable assets.\n\n"
			"Operations:\n"
			"- 'create': Create new DataTable (requires row_struct)\n"
			"- 'get_rows': Read rows (all or by row_name)\n"
			"- 'add_row': Add row with JSON properties\n"
			"- 'modify_row': Update existing row properties\n"
			"- 'delete_row': Remove a row by name\n"
			"- 'query': Get DataTable info (struct type, row count, row names)\n\n"
			"Row data is provided as a JSON object where keys match struct field names.\n"
			"Example: {\"Name\": \"Sword\", \"Damage\": 50, \"Weight\": 3.5}"
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation to perform (see description)"), true),
			FMCPToolParameter(TEXT("table_path"), TEXT("string"),
				TEXT("Path to DataTable asset"), false),
			FMCPToolParameter(TEXT("package_path"), TEXT("string"),
				TEXT("Package path for new DataTable (default: '/Game/Data/')"), false, TEXT("/Game/Data/")),
			FMCPToolParameter(TEXT("table_name"), TEXT("string"),
				TEXT("Name for new DataTable (for create)"), false),
			FMCPToolParameter(TEXT("row_struct"), TEXT("string"),
				TEXT("Row struct name (for create, e.g., 'TableRowBase')"), false),
			FMCPToolParameter(TEXT("row_name"), TEXT("string"),
				TEXT("Row name for get/add/modify/delete"), false),
			FMCPToolParameter(TEXT("row_data"), TEXT("object"),
				TEXT("JSON object with row property values"), false)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	FMCPToolResult ExecuteCreate(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteGetRows(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteAddRow(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteModifyRow(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteDeleteRow(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteQuery(const TSharedRef<FJsonObject>& Params);

	UDataTable* LoadDataTable(const FString& Path, FString& OutError);
	TSharedPtr<FJsonObject> RowToJson(UDataTable* Table, const FName& RowName);
};
