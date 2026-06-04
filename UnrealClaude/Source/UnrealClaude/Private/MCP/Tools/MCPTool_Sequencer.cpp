// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_Sequencer.h"
#include "UnrealClaudeModule.h"
#include "MCP/MCPParamValidator.h"

#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieSceneSection.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Misc/FrameRate.h"

namespace
{
	// Convert display-rate frame number to tick-resolution frame number
	FFrameNumber DisplayToTick(int32 DisplayFrame, FFrameRate DisplayRate, FFrameRate TickResolution)
	{
		FFrameTime TickTime = FFrameRate::TransformTime(FFrameTime(DisplayFrame), DisplayRate, TickResolution);
		return TickTime.FloorToFrame();
	}

	// Convert tick-resolution frame number to display-rate frame number
	int32 TickToDisplay(FFrameNumber TickFrame, FFrameRate DisplayRate, FFrameRate TickResolution)
	{
		FFrameTime DisplayTime = FFrameRate::TransformTime(FFrameTime(TickFrame), TickResolution, DisplayRate);
		return DisplayTime.FloorToFrame().Value;
	}

	ULevelSequence* LoadSequence(const FString& SeqPath)
	{
		FString AdjustedPath = SeqPath;
		if (!AdjustedPath.Contains(TEXT(".")))
		{
			AdjustedPath += TEXT(".") + FPaths::GetBaseFilename(SeqPath);
		}
		ULevelSequence* Seq = LoadObject<ULevelSequence>(nullptr, *AdjustedPath);
		if (!Seq) Seq = LoadObject<ULevelSequence>(nullptr, *SeqPath);
		return Seq;
	}
}

FMCPToolResult FMCPTool_Sequencer::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	if (Operation == TEXT("create")) return ExecuteCreate(Params);
	if (Operation == TEXT("add_actor_track")) return ExecuteAddActorTrack(Params);
	if (Operation == TEXT("add_keyframe")) return ExecuteAddKeyframe(Params);
	if (Operation == TEXT("query")) return ExecuteQuery(Params);
	if (Operation == TEXT("set_playback")) return ExecuteSetPlayback(Params);

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: %s. Valid: create, add_actor_track, add_keyframe, query, set_playback"),
		*Operation));
}

FMCPToolResult FMCPTool_Sequencer::ExecuteCreate(const TSharedRef<FJsonObject>& Params)
{
	FString SeqName;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("sequence_name"), SeqName, Error))
	{
		return Error.GetValue();
	}

	FString PackagePath = ExtractOptionalString(Params, TEXT("package_path"), TEXT("/Game/Cinematics/"));
	FString FullPath = PackagePath / SeqName;

	UPackage* Package = CreatePackage(*FullPath);
	if (!Package)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to create package: %s"), *FullPath));
	}

	ULevelSequence* NewSequence = NewObject<ULevelSequence>(Package, FName(*SeqName), RF_Public | RF_Standalone);
	if (!NewSequence)
	{
		return FMCPToolResult::Error(TEXT("Failed to create LevelSequence"));
	}

	NewSequence->Initialize();

	int32 FrameRate = ExtractOptionalNumber<int32>(Params, TEXT("frame_rate"), 30);
	UMovieScene* MovieScene = NewSequence->GetMovieScene();
	if (MovieScene)
	{
		FFrameRate DisplayRate(FrameRate, 1);
		MovieScene->SetDisplayRate(DisplayRate);

		int32 EndFrame = ExtractOptionalNumber<int32>(Params, TEXT("end_frame"), 150);
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		FFrameNumber StartTick = DisplayToTick(0, DisplayRate, TickResolution);
		FFrameNumber EndTick = DisplayToTick(EndFrame, DisplayRate, TickResolution);
		MovieScene->SetPlaybackRange(StartTick, (EndTick - StartTick).Value);
	}

	Package->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(FullPath, false);
	FAssetRegistryModule::AssetCreated(NewSequence);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("asset_path"), NewSequence->GetPathName());
	ResultData->SetStringField(TEXT("name"), SeqName);
	ResultData->SetNumberField(TEXT("frame_rate"), FrameRate);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Created LevelSequence '%s'"), *SeqName), ResultData);
}

FMCPToolResult FMCPTool_Sequencer::ExecuteAddActorTrack(const TSharedRef<FJsonObject>& Params)
{
	FString SeqPath, ActorName;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("sequence_path"), SeqPath, Error)) return Error.GetValue();
	if (!ExtractActorName(Params, TEXT("actor_name"), ActorName, Error)) return Error.GetValue();

	UWorld* World = nullptr;
	if (auto Err = ValidateEditorContext(World)) return Err.GetValue();

	ULevelSequence* Sequence = LoadSequence(SeqPath);
	if (!Sequence) return FMCPToolResult::Error(FString::Printf(TEXT("LevelSequence not found: %s"), *SeqPath));

	AActor* Actor = FindActorByNameOrLabel(World, ActorName);
	if (!Actor) return ActorNotFoundError(ActorName);

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene) return FMCPToolResult::Error(TEXT("Sequence has no MovieScene"));

	// Check if binding already exists
	for (int32 i = 0; i < MovieScene->GetPossessableCount(); ++i)
	{
		const FMovieScenePossessable& Poss = MovieScene->GetPossessable(i);
		if (Poss.GetName() == Actor->GetActorLabel() || Poss.GetName() == Actor->GetName())
		{
			TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
			ResultData->SetStringField(TEXT("binding_id"), Poss.GetGuid().ToString());
			ResultData->SetStringField(TEXT("actor_name"), Actor->GetName());
			ResultData->SetBoolField(TEXT("already_bound"), true);
			return FMCPToolResult::Success(TEXT("Actor already bound to sequence"), ResultData);
		}
	}

	FGuid BindingGuid = MovieScene->AddPossessable(Actor->GetActorLabel(), Actor->GetClass());
	Sequence->BindPossessableObject(BindingGuid, *Actor, Actor->GetWorld());

	// Add transform track
	UMovieScene3DTransformTrack* TransformTrack = MovieScene->AddTrack<UMovieScene3DTransformTrack>(BindingGuid);
	if (TransformTrack)
	{
		TransformTrack->AddSection(*TransformTrack->CreateNewSection());
	}

	Sequence->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(Sequence->GetOutermost()->GetPathName(), false);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("binding_id"), BindingGuid.ToString());
	ResultData->SetStringField(TEXT("actor_name"), Actor->GetName());
	ResultData->SetBoolField(TEXT("transform_track_added"), TransformTrack != nullptr);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Bound actor '%s' to sequence with transform track"), *Actor->GetName()),
		ResultData);
}

FMCPToolResult FMCPTool_Sequencer::ExecuteAddKeyframe(const TSharedRef<FJsonObject>& Params)
{
	FString SeqPath, ActorName;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("sequence_path"), SeqPath, Error)) return Error.GetValue();
	if (!ExtractActorName(Params, TEXT("actor_name"), ActorName, Error)) return Error.GetValue();

	double Frame;
	if (!Params->TryGetNumberField(TEXT("frame"), Frame))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: frame"));
	}

	ULevelSequence* Sequence = LoadSequence(SeqPath);
	if (!Sequence) return FMCPToolResult::Error(FString::Printf(TEXT("LevelSequence not found: %s"), *SeqPath));

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene) return FMCPToolResult::Error(TEXT("No MovieScene"));

	// Find binding for actor
	FGuid BindingGuid;
	bool bFound = false;
	for (int32 i = 0; i < MovieScene->GetPossessableCount(); ++i)
	{
		const FMovieScenePossessable& Poss = MovieScene->GetPossessable(i);
		if (Poss.GetName().Contains(ActorName, ESearchCase::IgnoreCase))
		{
			BindingGuid = Poss.GetGuid();
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Actor '%s' not bound to sequence. Use add_actor_track first."), *ActorName));
	}

	// Find transform track
	UMovieScene3DTransformTrack* TransformTrack = MovieScene->FindTrack<UMovieScene3DTransformTrack>(BindingGuid);
	if (!TransformTrack || TransformTrack->GetAllSections().Num() == 0)
	{
		return FMCPToolResult::Error(TEXT("No transform track found. Use add_actor_track first."));
	}

	UMovieScene3DTransformSection* Section = Cast<UMovieScene3DTransformSection>(TransformTrack->GetAllSections()[0]);
	if (!Section) return FMCPToolResult::Error(TEXT("Transform section not found"));

	// Convert display frame to tick resolution
	FFrameRate DisplayRate = MovieScene->GetDisplayRate();
	FFrameRate TickResolution = MovieScene->GetTickResolution();
	FFrameNumber FrameNumber = DisplayToTick(static_cast<int32>(Frame), DisplayRate, TickResolution);

	// Expand section range to include this keyframe
	Section->SetRange(TRange<FFrameNumber>::Hull(Section->GetRange(), TRange<FFrameNumber>(FrameNumber)));

	TArray<FString> KeyedChannels;

	TArrayView<FMovieSceneDoubleChannel*> Channels = Section->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
	// Channel order: TX, TY, TZ, RX, RY, RZ, SX, SY, SZ

	if (HasVectorParam(Params, TEXT("location")))
	{
		FVector Loc = ExtractVectorParam(Params, TEXT("location"));
		if (Channels.Num() > 2)
		{
			Channels[0]->AddCubicKey(FrameNumber, Loc.X);
			Channels[1]->AddCubicKey(FrameNumber, Loc.Y);
			Channels[2]->AddCubicKey(FrameNumber, Loc.Z);
			KeyedChannels.Add(FString::Printf(TEXT("location=(%.1f, %.1f, %.1f)"), Loc.X, Loc.Y, Loc.Z));
		}
	}

	if (HasVectorParam(Params, TEXT("rotation")))
	{
		FRotator Rot = ExtractRotatorParam(Params, TEXT("rotation"));
		if (Channels.Num() > 5)
		{
			Channels[3]->AddCubicKey(FrameNumber, Rot.Roll);
			Channels[4]->AddCubicKey(FrameNumber, Rot.Pitch);
			Channels[5]->AddCubicKey(FrameNumber, Rot.Yaw);
			KeyedChannels.Add(FString::Printf(TEXT("rotation=(%.1f, %.1f, %.1f)"), Rot.Pitch, Rot.Yaw, Rot.Roll));
		}
	}

	if (HasVectorParam(Params, TEXT("scale")))
	{
		FVector Scale = ExtractScaleParam(Params, TEXT("scale"));
		if (Channels.Num() > 8)
		{
			Channels[6]->AddCubicKey(FrameNumber, Scale.X);
			Channels[7]->AddCubicKey(FrameNumber, Scale.Y);
			Channels[8]->AddCubicKey(FrameNumber, Scale.Z);
			KeyedChannels.Add(FString::Printf(TEXT("scale=(%.1f, %.1f, %.1f)"), Scale.X, Scale.Y, Scale.Z));
		}
	}

	if (KeyedChannels.Num() == 0)
	{
		return FMCPToolResult::Error(TEXT("No transform data specified. Provide location, rotation, or scale."));
	}

	Sequence->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(Sequence->GetOutermost()->GetPathName(), false);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetNumberField(TEXT("frame"), Frame);
	ResultData->SetArrayField(TEXT("keyed"), StringArrayToJsonArray(KeyedChannels));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added keyframe at frame %d: %s"),
			static_cast<int32>(Frame), *FString::Join(KeyedChannels, TEXT(", "))),
		ResultData);
}

FMCPToolResult FMCPTool_Sequencer::ExecuteQuery(const TSharedRef<FJsonObject>& Params)
{
	FString SeqPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("sequence_path"), SeqPath, Error)) return Error.GetValue();

	ULevelSequence* Sequence = LoadSequence(SeqPath);
	if (!Sequence) return FMCPToolResult::Error(FString::Printf(TEXT("LevelSequence not found: %s"), *SeqPath));

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene) return FMCPToolResult::Error(TEXT("No MovieScene"));

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("sequence_path"), Sequence->GetPathName());
	ResultData->SetStringField(TEXT("name"), Sequence->GetName());
	ResultData->SetNumberField(TEXT("frame_rate"), MovieScene->GetDisplayRate().Numerator);

	// Playback range
	TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();
	FFrameRate DisplayRate = MovieScene->GetDisplayRate();
	FFrameRate TickResolution = MovieScene->GetTickResolution();

	if (Range.HasLowerBound() && Range.HasUpperBound())
	{
		int32 StartFrame = TickToDisplay(Range.GetLowerBoundValue(), DisplayRate, TickResolution);
		int32 EndFrame = TickToDisplay(Range.GetUpperBoundValue(), DisplayRate, TickResolution);
		ResultData->SetNumberField(TEXT("start_frame"), StartFrame);
		ResultData->SetNumberField(TEXT("end_frame"), EndFrame);
	}

	// Bindings
	TArray<TSharedPtr<FJsonValue>> Bindings;
	for (int32 i = 0; i < MovieScene->GetPossessableCount(); ++i)
	{
		const FMovieScenePossessable& Poss = MovieScene->GetPossessable(i);
		TSharedPtr<FJsonObject> BindObj = MakeShared<FJsonObject>();
		BindObj->SetStringField(TEXT("name"), Poss.GetName());
		BindObj->SetStringField(TEXT("guid"), Poss.GetGuid().ToString());
		BindObj->SetStringField(TEXT("class"), Poss.GetPossessedObjectClass() ? Poss.GetPossessedObjectClass()->GetName() : TEXT("Unknown"));

		// Collect tracks for this binding
		TArray<TSharedPtr<FJsonValue>> TrackArray;
		const FMovieSceneBinding* Binding = MovieScene->FindBinding(Poss.GetGuid());
		if (Binding)
		{
			for (UMovieSceneTrack* Track : Binding->GetTracks())
			{
				if (!Track) continue;
				TSharedPtr<FJsonObject> TrackObj = MakeShared<FJsonObject>();
				TrackObj->SetStringField(TEXT("type"), Track->GetClass()->GetName());
				TrackObj->SetNumberField(TEXT("section_count"), Track->GetAllSections().Num());
				TrackArray.Add(MakeShared<FJsonValueObject>(TrackObj));
			}
		}
		BindObj->SetArrayField(TEXT("tracks"), TrackArray);

		Bindings.Add(MakeShared<FJsonValueObject>(BindObj));
	}
	ResultData->SetArrayField(TEXT("bindings"), Bindings);
	ResultData->SetNumberField(TEXT("binding_count"), Bindings.Num());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Sequence '%s': %d bindings, %d fps"),
			*Sequence->GetName(), Bindings.Num(), MovieScene->GetDisplayRate().Numerator),
		ResultData);
}

FMCPToolResult FMCPTool_Sequencer::ExecuteSetPlayback(const TSharedRef<FJsonObject>& Params)
{
	FString SeqPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("sequence_path"), SeqPath, Error)) return Error.GetValue();

	ULevelSequence* Sequence = LoadSequence(SeqPath);
	if (!Sequence) return FMCPToolResult::Error(FString::Printf(TEXT("LevelSequence not found: %s"), *SeqPath));

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene) return FMCPToolResult::Error(TEXT("No MovieScene"));

	TArray<FString> Changes;

	int32 FrameRate = ExtractOptionalNumber<int32>(Params, TEXT("frame_rate"), 0);
	if (FrameRate > 0)
	{
		MovieScene->SetDisplayRate(FFrameRate(FrameRate, 1));
		Changes.Add(FString::Printf(TEXT("frame_rate=%d"), FrameRate));
	}

	double StartFrameD, EndFrameD;
	bool bHasStart = Params->TryGetNumberField(TEXT("start_frame"), StartFrameD);
	bool bHasEnd = Params->TryGetNumberField(TEXT("end_frame"), EndFrameD);

	if (bHasStart || bHasEnd)
	{
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		FFrameRate DisplayRate = MovieScene->GetDisplayRate();

		FFrameNumber Start = bHasStart ?
			DisplayToTick(static_cast<int32>(StartFrameD), DisplayRate, TickResolution) :
			MovieScene->GetPlaybackRange().GetLowerBoundValue();

		FFrameNumber End = bHasEnd ?
			DisplayToTick(static_cast<int32>(EndFrameD), DisplayRate, TickResolution) :
			MovieScene->GetPlaybackRange().GetUpperBoundValue();

		MovieScene->SetPlaybackRange(Start, (End - Start).Value);
		if (bHasStart) Changes.Add(FString::Printf(TEXT("start_frame=%d"), static_cast<int32>(StartFrameD)));
		if (bHasEnd) Changes.Add(FString::Printf(TEXT("end_frame=%d"), static_cast<int32>(EndFrameD)));
	}

	if (Changes.Num() == 0)
	{
		return FMCPToolResult::Error(TEXT("No playback settings specified. Provide frame_rate, start_frame, or end_frame."));
	}

	Sequence->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(Sequence->GetOutermost()->GetPathName(), false);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetArrayField(TEXT("changes"), StringArrayToJsonArray(Changes));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Updated playback: %s"), *FString::Join(Changes, TEXT(", "))),
		ResultData);
}
