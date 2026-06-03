// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_Level.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"
#include "UnrealClaudeUtils.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Selection.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"

FMCPToolResult FMCPTool_Level::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	const FString OpLower = Operation.ToLower();

	if (OpLower == TEXT("save"))              return ExecuteSave(Params);
	if (OpLower == TEXT("get_actor_bounds"))  return ExecuteGetActorBounds(Params);
	if (OpLower == TEXT("select_actors"))     return ExecuteSelectActors(Params);
	if (OpLower == TEXT("focus_viewport"))    return ExecuteFocusViewport(Params);

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: '%s'. Valid: save, get_actor_bounds, select_actors, focus_viewport"), *Operation));
}

FMCPToolResult FMCPTool_Level::ExecuteSave(const TSharedRef<FJsonObject>& Params)
{
	UWorld* World = nullptr;
	if (auto Err = ValidateEditorContext(World)) return Err.GetValue();

	const bool bSaved = FEditorFileUtils::SaveCurrentLevel();

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetBoolField(TEXT("saved"), bSaved);

	if (!bSaved)
	{
		return FMCPToolResult::Error(TEXT("Failed to save level — check Output Log for details"));
	}

	if (ULevel* Level = World->GetCurrentLevel())
	{
		ResultData->SetStringField(TEXT("level"), Level->GetOutermost()->GetName());
	}

	return FMCPToolResult::Success(TEXT("Level saved"), ResultData);
}

FMCPToolResult FMCPTool_Level::ExecuteGetActorBounds(const TSharedRef<FJsonObject>& Params)
{
	UWorld* World = nullptr;
	if (auto Err = ValidateEditorContext(World)) return Err.GetValue();

	FString ActorName;
	TOptional<FMCPToolResult> Error;
	if (!ExtractActorName(Params, TEXT("actor_name"), ActorName, Error))
	{
		return Error.GetValue();
	}

	AActor* Actor = FindActorByNameOrLabel(World, ActorName);
	if (!Actor) return ActorNotFoundError(ActorName);

	FVector Origin, BoxExtent;
	Actor->GetActorBounds(/*bOnlyCollidingComponents=*/false, Origin, BoxExtent);

	const FVector Min = Origin - BoxExtent;
	const FVector Max = Origin + BoxExtent;
	const FVector Center = Actor->GetActorLocation();

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("actor"),   Actor->GetName());
	ResultData->SetObjectField(TEXT("origin"),  UnrealClaudeJsonUtils::VectorToJson(Origin));
	ResultData->SetObjectField(TEXT("extent"),  UnrealClaudeJsonUtils::VectorToJson(BoxExtent));
	ResultData->SetObjectField(TEXT("min"),     UnrealClaudeJsonUtils::VectorToJson(Min));
	ResultData->SetObjectField(TEXT("max"),     UnrealClaudeJsonUtils::VectorToJson(Max));
	ResultData->SetObjectField(TEXT("center"),  UnrealClaudeJsonUtils::VectorToJson(Center));
	ResultData->SetStringField(TEXT("tip"),
		TEXT("Use 'origin' to place particles at geometric center of actor"));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Got bounds for '%s'"), *ActorName), ResultData);
}

FMCPToolResult FMCPTool_Level::ExecuteSelectActors(const TSharedRef<FJsonObject>& Params)
{
	UWorld* World = nullptr;
	if (auto Err = ValidateEditorContext(World)) return Err.GetValue();

	const FString ActorName    = ExtractOptionalString(Params, TEXT("actor_name"));
	const FString ClassFilter  = ExtractOptionalString(Params, TEXT("class_filter"));
	const FString NameFilter   = ExtractOptionalString(Params, TEXT("name_filter"));
	const bool bAddToSelection = ExtractOptionalBool(Params, TEXT("add_to_selection"), false);

	if (!bAddToSelection)
	{
		GEditor->SelectNone(/*bNoteSelectionChange=*/true, /*bDeselectBSP=*/true, /*bWarnAboutTooManyActors=*/false);
	}

	TArray<FString> Selected;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor->IsHidden()) continue;

		const FString Name  = Actor->GetName();
		const FString Label = Actor->GetActorLabel();

		// Match by exact actor_name first
		if (!ActorName.IsEmpty())
		{
			if (!Name.Equals(ActorName, ESearchCase::IgnoreCase) &&
				!Label.Equals(ActorName, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}
		else
		{
			// Apply class + name filters
			if (!ClassFilter.IsEmpty() && !Actor->GetClass()->GetName().Contains(ClassFilter, ESearchCase::IgnoreCase))
				continue;
			if (!NameFilter.IsEmpty() &&
				!Name.Contains(NameFilter, ESearchCase::IgnoreCase) &&
				!Label.Contains(NameFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		GEditor->SelectActor(Actor, /*bInSelected=*/true, /*bNotify=*/false);
		Selected.Add(Label.IsEmpty() ? Name : Label);
	}

	GEditor->NoteSelectionChange();

	TArray<TSharedPtr<FJsonValue>> SelectedArr;
	for (const FString& S : Selected) SelectedArr.Add(MakeShared<FJsonValueString>(S));

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetArrayField(TEXT("selected"), SelectedArr);
	ResultData->SetNumberField(TEXT("count"),    Selected.Num());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Selected %d actor(s)"), Selected.Num()), ResultData);
}

FMCPToolResult FMCPTool_Level::ExecuteFocusViewport(const TSharedRef<FJsonObject>& Params)
{
	UWorld* World = nullptr;
	if (auto Err = ValidateEditorContext(World)) return Err.GetValue();

	FString ActorName;
	TOptional<FMCPToolResult> Error;
	if (!ExtractActorName(Params, TEXT("actor_name"), ActorName, Error))
	{
		return Error.GetValue();
	}

	AActor* Actor = FindActorByNameOrLabel(World, ActorName);
	if (!Actor) return ActorNotFoundError(ActorName);

	// Select then move viewport cameras to actor
	GEditor->SelectNone(true, true, false);
	GEditor->SelectActor(Actor, true, true);
	GEditor->MoveViewportCamerasToActor(*Actor, /*bActiveViewportOnly=*/false);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("actor"),    Actor->GetName());
	ResultData->SetStringField(TEXT("label"),    Actor->GetActorLabel());
	ResultData->SetObjectField(TEXT("location"), UnrealClaudeJsonUtils::VectorToJson(Actor->GetActorLocation()));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Focused viewport on '%s'"), *ActorName), ResultData);
}
