// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Create C++ Class
 *
 * Scaffold a new C++ class with correct UE5 boilerplate.
 * Generates header + source files and registers the module in Build.cs.
 *
 * Supports common base classes: AActor, APawn, ACharacter, AGameModeBase,
 * APlayerController, UActorComponent, USceneComponent, UObject, UUserWidget, etc.
 */
class FMCPTool_CreateCppClass : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("create_cpp_class");
		Info.Description = TEXT(
			"Scaffold a new C++ class with UE5 boilerplate.\n\n"
			"Generates .h and .cpp files with proper includes, UCLASS macro, constructor, and "
			"BeginPlay/Tick stubs for Actor-derived classes.\n\n"
			"Base class shortcuts (case-insensitive):\n"
			"- 'Actor' -> AActor\n"
			"- 'Pawn' -> APawn\n"
			"- 'Character' -> ACharacter\n"
			"- 'GameModeBase' -> AGameModeBase\n"
			"- 'GameMode' -> AGameMode\n"
			"- 'PlayerController' -> APlayerController\n"
			"- 'ActorComponent' -> UActorComponent\n"
			"- 'SceneComponent' -> USceneComponent\n"
			"- 'Object' -> UObject\n"
			"- 'UserWidget' -> UUserWidget\n"
			"- 'GameInstance' -> UGameInstance\n"
			"- 'HUD' -> AHUD\n\n"
			"Files are placed in Source/<ModuleName>/ under the project root.\n"
			"Optionally creates a subfolder (e.g., Source/<Module>/Player/)."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("class_name"), TEXT("string"),
				TEXT("Name of the new class (e.g., 'MyPlayerPawn'). Prefix A/U is auto-added if missing."), true),
			FMCPToolParameter(TEXT("base_class"), TEXT("string"),
				TEXT("Base class name or shortcut (see description). Default: 'Actor'"), false, TEXT("Actor")),
			FMCPToolParameter(TEXT("module_name"), TEXT("string"),
				TEXT("Module name (default: project name from .uproject)"), false),
			FMCPToolParameter(TEXT("subfolder"), TEXT("string"),
				TEXT("Optional subfolder under Source/<Module>/ (e.g., 'Player', 'Weapons')"), false),
			FMCPToolParameter(TEXT("include_tick"), TEXT("boolean"),
				TEXT("Include Tick() override for Actor-derived classes (default: true)"), false, TEXT("true")),
			FMCPToolParameter(TEXT("include_components"), TEXT("boolean"),
				TEXT("Include root component setup for Actor-derived classes (default: false)"), false, TEXT("false")),
			FMCPToolParameter(TEXT("extra_includes"), TEXT("array"),
				TEXT("Additional #include paths to add to the header"), false),
			FMCPToolParameter(TEXT("extra_uproperties"), TEXT("array"),
				TEXT("Additional UPROPERTY declarations [{name, type, category, specifiers}]"), false),
			FMCPToolParameter(TEXT("dry_run"), TEXT("boolean"),
				TEXT("Preview generated code without writing files (default: false)"), false, TEXT("false"))
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	struct FBaseClassInfo
	{
		FString FullName;
		FString HeaderInclude;
		FString Module;
		bool bIsActor;
		bool bIsPawn;
		bool bIsComponent;
	};

	FBaseClassInfo ResolveBaseClass(const FString& Input);
	FString GenerateHeaderContent(const FString& ClassName, const FString& ApiMacro,
		const FBaseClassInfo& BaseInfo, const TSharedRef<FJsonObject>& Params);
	FString GenerateCppContent(const FString& ClassName, const FBaseClassInfo& BaseInfo,
		const TSharedRef<FJsonObject>& Params);
	FString GetProjectModuleName();
};
