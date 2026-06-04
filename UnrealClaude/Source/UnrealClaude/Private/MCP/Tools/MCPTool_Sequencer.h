// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Sequencer / Level Sequence
 *
 * Create and edit Level Sequences for cinematic playback.
 *
 * Operations:
 *   - create: Create a new LevelSequence asset
 *   - add_actor_track: Bind an actor to the sequence
 *   - add_keyframe: Add a transform keyframe for a bound actor
 *   - query: List tracks and keyframes in a sequence
 *   - set_playback: Configure playback range and rate
 */
class FMCPTool_Sequencer : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("sequencer");
		Info.Description = TEXT(
			"Create and edit Level Sequences (cinematics).\n\n"
			"Operations:\n"
			"- 'create': Create new LevelSequence asset\n"
			"- 'add_actor_track': Bind an actor to the sequence for animation\n"
			"- 'add_keyframe': Add transform keyframe at a specific frame\n"
			"- 'query': List all tracks, bindings, and keyframe counts\n"
			"- 'set_playback': Set playback range and frame rate\n\n"
			"Keyframes use frame numbers (not seconds). Default frame rate is 30fps.\n"
			"Use set_playback to change frame rate before adding keyframes."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation to perform"), true),
			FMCPToolParameter(TEXT("sequence_path"), TEXT("string"),
				TEXT("Path to existing LevelSequence asset"), false),
			FMCPToolParameter(TEXT("package_path"), TEXT("string"),
				TEXT("Package path for new sequence (default: '/Game/Cinematics/')"), false, TEXT("/Game/Cinematics/")),
			FMCPToolParameter(TEXT("sequence_name"), TEXT("string"),
				TEXT("Name for new LevelSequence (for create)"), false),
			FMCPToolParameter(TEXT("actor_name"), TEXT("string"),
				TEXT("Actor name/label to bind (for add_actor_track)"), false),
			FMCPToolParameter(TEXT("frame"), TEXT("number"),
				TEXT("Frame number for keyframe"), false),
			FMCPToolParameter(TEXT("location"), TEXT("object"),
				TEXT("Location {x, y, z} for transform keyframe"), false),
			FMCPToolParameter(TEXT("rotation"), TEXT("object"),
				TEXT("Rotation {pitch, yaw, roll} for transform keyframe"), false),
			FMCPToolParameter(TEXT("scale"), TEXT("object"),
				TEXT("Scale {x, y, z} for transform keyframe"), false),
			FMCPToolParameter(TEXT("start_frame"), TEXT("number"),
				TEXT("Playback start frame (for set_playback)"), false),
			FMCPToolParameter(TEXT("end_frame"), TEXT("number"),
				TEXT("Playback end frame (for set_playback)"), false),
			FMCPToolParameter(TEXT("frame_rate"), TEXT("number"),
				TEXT("Frame rate in fps (for set_playback, default: 30)"), false, TEXT("30"))
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	FMCPToolResult ExecuteCreate(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteAddActorTrack(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteAddKeyframe(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteQuery(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteSetPlayback(const TSharedRef<FJsonObject>& Params);
};
