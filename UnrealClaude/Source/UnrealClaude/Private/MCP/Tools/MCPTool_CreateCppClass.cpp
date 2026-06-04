// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_CreateCppClass.h"
#include "UnrealClaudeModule.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

FMCPToolResult FMCPTool_CreateCppClass::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString ClassName;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("class_name"), ClassName, Error))
	{
		return Error.GetValue();
	}

	FString BaseClassInput = ExtractOptionalString(Params, TEXT("base_class"), TEXT("Actor"));
	FBaseClassInfo BaseInfo = ResolveBaseClass(BaseClassInput);

	if (BaseInfo.FullName.IsEmpty())
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Unknown base class: %s. Use: Actor, Pawn, Character, GameModeBase, GameMode, "
				"PlayerController, ActorComponent, SceneComponent, Object, UserWidget, GameInstance, HUD"),
			*BaseClassInput));
	}

	// Auto-add prefix if missing
	TCHAR ExpectedPrefix = BaseInfo.bIsActor || BaseInfo.FullName.StartsWith(TEXT("A")) ? 'A' : 'U';
	if (BaseInfo.bIsComponent || BaseInfo.FullName.StartsWith(TEXT("U")))
	{
		ExpectedPrefix = 'U';
	}
	if (ClassName[0] != 'A' && ClassName[0] != 'U')
	{
		ClassName = FString::Chr(ExpectedPrefix) + ClassName;
	}

	FString ModuleName = ExtractOptionalString(Params, TEXT("module_name"));
	if (ModuleName.IsEmpty())
	{
		ModuleName = GetProjectModuleName();
	}

	FString Subfolder = ExtractOptionalString(Params, TEXT("subfolder"));

	// Build file paths
	FString SourceDir = FPaths::ProjectDir() / TEXT("Source") / ModuleName;
	if (!Subfolder.IsEmpty())
	{
		SourceDir = SourceDir / Subfolder;
	}

	FString HeaderPath = SourceDir / (ClassName + TEXT(".h"));
	FString CppPath = SourceDir / (ClassName + TEXT(".cpp"));

	// API macro: ModuleName uppercased + _API
	FString ApiMacro = ModuleName.ToUpper() + TEXT("_API");

	FString HeaderContent = GenerateHeaderContent(ClassName, ApiMacro, BaseInfo, Params);
	FString CppContent = GenerateCppContent(ClassName, BaseInfo, Params);

	bool bDryRun = ExtractOptionalBool(Params, TEXT("dry_run"), false);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("class_name"), ClassName);
	ResultData->SetStringField(TEXT("base_class"), BaseInfo.FullName);
	ResultData->SetStringField(TEXT("module_name"), ModuleName);
	ResultData->SetStringField(TEXT("header_path"), HeaderPath);
	ResultData->SetStringField(TEXT("cpp_path"), CppPath);

	if (bDryRun)
	{
		ResultData->SetStringField(TEXT("header_content"), HeaderContent);
		ResultData->SetStringField(TEXT("cpp_content"), CppContent);
		ResultData->SetBoolField(TEXT("dry_run"), true);
		return FMCPToolResult::Success(TEXT("Dry run — preview generated code"), ResultData);
	}

	// Check if files already exist
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (PlatformFile.FileExists(*HeaderPath))
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Header already exists: %s"), *HeaderPath));
	}
	if (PlatformFile.FileExists(*CppPath))
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Source file already exists: %s"), *CppPath));
	}

	// Create directory if needed
	PlatformFile.CreateDirectoryTree(*SourceDir);

	// Write files
	if (!FFileHelper::SaveStringToFile(HeaderContent, *HeaderPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to write header: %s"), *HeaderPath));
	}
	if (!FFileHelper::SaveStringToFile(CppContent, *CppPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to write source: %s"), *CppPath));
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Created C++ class %s (%s + %s)"), *ClassName, *HeaderPath, *CppPath);

	ResultData->SetBoolField(TEXT("header_written"), true);
	ResultData->SetBoolField(TEXT("cpp_written"), true);

	FMCPToolResult Result = FMCPToolResult::Success(
		FString::Printf(TEXT("Created C++ class '%s' : %s"), *ClassName, *BaseInfo.FullName),
		ResultData);
	Result.Warnings.Add(TEXT("Trigger 'build hot_reload' or restart editor to compile the new class."));
	return Result;
}

FMCPTool_CreateCppClass::FBaseClassInfo FMCPTool_CreateCppClass::ResolveBaseClass(const FString& Input)
{
	FBaseClassInfo Info;
	Info.bIsActor = false;
	Info.bIsPawn = false;
	Info.bIsComponent = false;

	FString Lower = Input.ToLower();

	// Strip prefix if present
	if (Lower.StartsWith(TEXT("a")) && Lower.Len() > 1 && FChar::IsUpper(Input[1]))
	{
		Lower = Lower.Mid(1);
	}
	else if (Lower.StartsWith(TEXT("u")) && Lower.Len() > 1 && FChar::IsUpper(Input[1]))
	{
		Lower = Lower.Mid(1);
	}

	if (Lower == TEXT("actor"))
	{
		Info.FullName = TEXT("AActor");
		Info.HeaderInclude = TEXT("GameFramework/Actor.h");
		Info.Module = TEXT("Engine");
		Info.bIsActor = true;
	}
	else if (Lower == TEXT("pawn"))
	{
		Info.FullName = TEXT("APawn");
		Info.HeaderInclude = TEXT("GameFramework/Pawn.h");
		Info.Module = TEXT("Engine");
		Info.bIsActor = true;
		Info.bIsPawn = true;
	}
	else if (Lower == TEXT("character"))
	{
		Info.FullName = TEXT("ACharacter");
		Info.HeaderInclude = TEXT("GameFramework/Character.h");
		Info.Module = TEXT("Engine");
		Info.bIsActor = true;
		Info.bIsPawn = true;
	}
	else if (Lower == TEXT("gamemodebase"))
	{
		Info.FullName = TEXT("AGameModeBase");
		Info.HeaderInclude = TEXT("GameFramework/GameModeBase.h");
		Info.Module = TEXT("Engine");
		Info.bIsActor = true;
	}
	else if (Lower == TEXT("gamemode"))
	{
		Info.FullName = TEXT("AGameMode");
		Info.HeaderInclude = TEXT("GameFramework/GameMode.h");
		Info.Module = TEXT("Engine");
		Info.bIsActor = true;
	}
	else if (Lower == TEXT("playercontroller"))
	{
		Info.FullName = TEXT("APlayerController");
		Info.HeaderInclude = TEXT("GameFramework/PlayerController.h");
		Info.Module = TEXT("Engine");
		Info.bIsActor = true;
	}
	else if (Lower == TEXT("actorcomponent"))
	{
		Info.FullName = TEXT("UActorComponent");
		Info.HeaderInclude = TEXT("Components/ActorComponent.h");
		Info.Module = TEXT("Engine");
		Info.bIsComponent = true;
	}
	else if (Lower == TEXT("scenecomponent"))
	{
		Info.FullName = TEXT("USceneComponent");
		Info.HeaderInclude = TEXT("Components/SceneComponent.h");
		Info.Module = TEXT("Engine");
		Info.bIsComponent = true;
	}
	else if (Lower == TEXT("object"))
	{
		Info.FullName = TEXT("UObject");
		Info.HeaderInclude = TEXT("UObject/Object.h");
		Info.Module = TEXT("CoreUObject");
	}
	else if (Lower == TEXT("userwidget") || Lower == TEXT("widget"))
	{
		Info.FullName = TEXT("UUserWidget");
		Info.HeaderInclude = TEXT("Blueprint/UserWidget.h");
		Info.Module = TEXT("UMG");
	}
	else if (Lower == TEXT("gameinstance"))
	{
		Info.FullName = TEXT("UGameInstance");
		Info.HeaderInclude = TEXT("Engine/GameInstance.h");
		Info.Module = TEXT("Engine");
	}
	else if (Lower == TEXT("hud"))
	{
		Info.FullName = TEXT("AHUD");
		Info.HeaderInclude = TEXT("GameFramework/HUD.h");
		Info.Module = TEXT("Engine");
		Info.bIsActor = true;
	}
	else if (Lower == TEXT("info"))
	{
		Info.FullName = TEXT("AInfo");
		Info.HeaderInclude = TEXT("GameFramework/Info.h");
		Info.Module = TEXT("Engine");
		Info.bIsActor = true;
	}
	else if (Lower == TEXT("gamestate") || Lower == TEXT("gamestatebase"))
	{
		Info.FullName = TEXT("AGameStateBase");
		Info.HeaderInclude = TEXT("GameFramework/GameStateBase.h");
		Info.Module = TEXT("Engine");
		Info.bIsActor = true;
	}
	else if (Lower == TEXT("playerstate"))
	{
		Info.FullName = TEXT("APlayerState");
		Info.HeaderInclude = TEXT("GameFramework/PlayerState.h");
		Info.Module = TEXT("Engine");
		Info.bIsActor = true;
	}

	return Info;
}

FString FMCPTool_CreateCppClass::GenerateHeaderContent(const FString& ClassName, const FString& ApiMacro,
	const FBaseClassInfo& BaseInfo, const TSharedRef<FJsonObject>& Params)
{
	FString Content;

	Content += TEXT("#pragma once\n\n");
	Content += TEXT("#include \"CoreMinimal.h\"\n");
	Content += FString::Printf(TEXT("#include \"%s\"\n"), *BaseInfo.HeaderInclude);

	// Extra includes
	const TArray<TSharedPtr<FJsonValue>>* ExtraIncludes;
	if (Params->TryGetArrayField(TEXT("extra_includes"), ExtraIncludes))
	{
		for (const auto& Inc : *ExtraIncludes)
		{
			FString IncPath;
			if (Inc->TryGetString(IncPath))
			{
				Content += FString::Printf(TEXT("#include \"%s\"\n"), *IncPath);
			}
		}
	}

	bool bIncludeComponents = ExtractOptionalBool(Params, TEXT("include_components"), false);
	if (bIncludeComponents && BaseInfo.bIsActor)
	{
		Content += TEXT("#include \"Components/SceneComponent.h\"\n");
	}

	Content += FString::Printf(TEXT("#include \"%s.generated.h\"\n\n"), *ClassName);

	Content += TEXT("UCLASS()\n");
	Content += FString::Printf(TEXT("class %s %s : public %s\n"), *ApiMacro, *ClassName, *BaseInfo.FullName);
	Content += TEXT("{\n");
	Content += TEXT("\tGENERATED_BODY()\n\n");
	Content += TEXT("public:\n");
	Content += FString::Printf(TEXT("\t%s();\n\n"), *ClassName);

	if (BaseInfo.bIsActor)
	{
		Content += TEXT("protected:\n");
		Content += TEXT("\tvirtual void BeginPlay() override;\n\n");

		bool bIncludeTick = ExtractOptionalBool(Params, TEXT("include_tick"), true);
		if (bIncludeTick)
		{
			Content += TEXT("public:\n");
			Content += TEXT("\tvirtual void Tick(float DeltaTime) override;\n\n");
		}
	}

	if (BaseInfo.bIsComponent)
	{
		Content += TEXT("protected:\n");
		Content += TEXT("\tvirtual void BeginPlay() override;\n\n");
		Content += TEXT("public:\n");
		Content += TEXT("\tvirtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;\n\n");
	}

	// Root component
	if (bIncludeComponents && BaseInfo.bIsActor)
	{
		Content += TEXT("protected:\n");
		Content += TEXT("\tUPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = \"Components\")\n");
		Content += TEXT("\tUSceneComponent* DefaultSceneRoot;\n\n");
	}

	// Extra UPROPERTY declarations
	const TArray<TSharedPtr<FJsonValue>>* ExtraProps;
	if (Params->TryGetArrayField(TEXT("extra_uproperties"), ExtraProps))
	{
		if (!bIncludeComponents || !BaseInfo.bIsActor)
		{
			Content += TEXT("protected:\n");
		}
		for (const auto& PropVal : *ExtraProps)
		{
			const TSharedPtr<FJsonObject>* PropObj;
			if (PropVal->TryGetObject(PropObj) && PropObj && (*PropObj).IsValid())
			{
				FString PropName, PropType, Category, Specifiers;
				(*PropObj)->TryGetStringField(TEXT("name"), PropName);
				(*PropObj)->TryGetStringField(TEXT("type"), PropType);
				(*PropObj)->TryGetStringField(TEXT("category"), Category);
				(*PropObj)->TryGetStringField(TEXT("specifiers"), Specifiers);

				if (Category.IsEmpty()) Category = TEXT("Default");
				if (Specifiers.IsEmpty()) Specifiers = TEXT("EditAnywhere, BlueprintReadWrite");

				if (!PropName.IsEmpty() && !PropType.IsEmpty())
				{
					Content += FString::Printf(TEXT("\tUPROPERTY(%s, Category = \"%s\")\n"), *Specifiers, *Category);
					Content += FString::Printf(TEXT("\t%s %s;\n\n"), *PropType, *PropName);
				}
			}
		}
	}

	Content += TEXT("};\n");

	return Content;
}

FString FMCPTool_CreateCppClass::GenerateCppContent(const FString& ClassName, const FBaseClassInfo& BaseInfo,
	const TSharedRef<FJsonObject>& Params)
{
	FString Content;

	Content += FString::Printf(TEXT("#include \"%s.h\"\n\n"), *ClassName);

	// Constructor
	Content += FString::Printf(TEXT("%s::%s()\n{\n"), *ClassName, *ClassName);

	if (BaseInfo.bIsActor)
	{
		Content += TEXT("\tPrimaryActorTick.bCanEverTick = ");
		bool bIncludeTick = ExtractOptionalBool(Params, TEXT("include_tick"), true);
		Content += bIncludeTick ? TEXT("true;\n") : TEXT("false;\n");

		bool bIncludeComponents = ExtractOptionalBool(Params, TEXT("include_components"), false);
		if (bIncludeComponents)
		{
			Content += TEXT("\n\tDefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT(\"DefaultSceneRoot\"));\n");
			Content += TEXT("\tRootComponent = DefaultSceneRoot;\n");
		}
	}

	if (BaseInfo.bIsComponent)
	{
		Content += TEXT("\tPrimaryComponentTick.bCanEverTick = true;\n");
	}

	Content += TEXT("}\n\n");

	// BeginPlay
	if (BaseInfo.bIsActor || BaseInfo.bIsComponent)
	{
		Content += FString::Printf(TEXT("void %s::BeginPlay()\n{\n"), *ClassName);
		Content += TEXT("\tSuper::BeginPlay();\n");
		Content += TEXT("}\n\n");
	}

	// Tick
	if (BaseInfo.bIsActor)
	{
		bool bIncludeTick = ExtractOptionalBool(Params, TEXT("include_tick"), true);
		if (bIncludeTick)
		{
			Content += FString::Printf(TEXT("void %s::Tick(float DeltaTime)\n{\n"), *ClassName);
			Content += TEXT("\tSuper::Tick(DeltaTime);\n");
			Content += TEXT("}\n");
		}
	}

	if (BaseInfo.bIsComponent)
	{
		Content += FString::Printf(TEXT("void %s::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)\n{\n"), *ClassName);
		Content += TEXT("\tSuper::TickComponent(DeltaTime, TickType, ThisTickFunction);\n");
		Content += TEXT("}\n");
	}

	return Content;
}

FString FMCPTool_CreateCppClass::GetProjectModuleName()
{
	FString ProjectPath = FPaths::GetProjectFilePath();
	FString ProjectName = FPaths::GetBaseFilename(ProjectPath);
	return ProjectName;
}
