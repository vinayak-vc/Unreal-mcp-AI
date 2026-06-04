// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Import Asset
 *
 * Import external files (FBX, PNG, WAV, PLY, etc.) into Content Browser.
 *
 * Operations:
 *   - import: Import a file from disk into the project
 *   - bulk_import: Import multiple files at once
 */
class FMCPTool_ImportAsset : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("import_asset");
		Info.Description = TEXT(
			"Import external files into the Content Browser.\n\n"
			"Operations:\n"
			"- 'import': Import a single file from disk\n"
			"- 'bulk_import': Import multiple files at once\n\n"
			"Supported file types (auto-detected by extension):\n"
			"- Meshes: .fbx, .obj, .gltf, .glb\n"
			"- Textures: .png, .jpg, .jpeg, .tga, .bmp, .exr, .hdr\n"
			"- Audio: .wav, .ogg\n"
			"- Data: .csv (DataTable), .json\n"
			"- Other: any format with a registered UE import factory\n\n"
			"The source_path must be an absolute path on disk.\n"
			"The destination_path is a Content Browser path (e.g., '/Game/Textures/')."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation: 'import' or 'bulk_import'"), true),
			FMCPToolParameter(TEXT("source_path"), TEXT("string"),
				TEXT("Absolute path to file on disk (for import)"), false),
			FMCPToolParameter(TEXT("source_paths"), TEXT("array"),
				TEXT("Array of absolute file paths (for bulk_import)"), false),
			FMCPToolParameter(TEXT("destination_path"), TEXT("string"),
				TEXT("Content Browser destination path (default: '/Game/Imported/')"), false, TEXT("/Game/Imported/")),
			FMCPToolParameter(TEXT("replace_existing"), TEXT("boolean"),
				TEXT("Replace existing assets with same name (default: true)"), false, TEXT("true")),
			FMCPToolParameter(TEXT("auto_create_folder"), TEXT("boolean"),
				TEXT("Create destination folder if it doesn't exist (default: true)"), false, TEXT("true"))
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	FMCPToolResult ExecuteImport(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteBulkImport(const TSharedRef<FJsonObject>& Params);

	TSharedPtr<FJsonObject> ImportSingleFile(const FString& SourcePath, const FString& DestPath,
		bool bReplace, FString& OutError);
};
