// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Collision Profile / Collision Settings
 *
 * Get and set collision presets and channel responses on actor components.
 *
 * Operations:
 *   - get: Get collision settings for an actor's component
 *   - set_profile: Set collision profile preset name
 *   - set_response: Set individual channel responses
 *   - list_profiles: List all available collision profiles
 *   - list_channels: List all collision channels
 */
class FMCPTool_CollisionProfile : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("collision");
		Info.Description = TEXT(
			"Get and set collision presets on actor components.\n\n"
			"Operations:\n"
			"- 'get': Get current collision settings for actor's component\n"
			"- 'set_profile': Apply a collision profile preset\n"
			"- 'set_response': Set individual channel responses\n"
			"- 'list_profiles': List all available collision profiles\n"
			"- 'list_channels': List all collision channels\n\n"
			"Common profiles: NoCollision, BlockAll, OverlapAll, BlockAllDynamic,\n"
			"OverlapAllDynamic, IgnoreOnlyPawn, OverlapOnlyPawn, Pawn, Spectator,\n"
			"CharacterMesh, PhysicsActor, Destructible, InvisibleWall, etc.\n\n"
			"Response types: 'Ignore', 'Overlap', 'Block'"
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation to perform"), true),
			FMCPToolParameter(TEXT("actor_name"), TEXT("string"),
				TEXT("Actor name or label"), false),
			FMCPToolParameter(TEXT("component_name"), TEXT("string"),
				TEXT("Specific component name (default: root component)"), false),
			FMCPToolParameter(TEXT("profile_name"), TEXT("string"),
				TEXT("Collision profile preset name (for set_profile)"), false),
			FMCPToolParameter(TEXT("channel"), TEXT("string"),
				TEXT("Collision channel name (for set_response)"), false),
			FMCPToolParameter(TEXT("response"), TEXT("string"),
				TEXT("Response type: 'Ignore', 'Overlap', 'Block'"), false),
			FMCPToolParameter(TEXT("responses"), TEXT("object"),
				TEXT("Batch responses: {\"channel_name\": \"response_type\", ...}"), false),
			FMCPToolParameter(TEXT("enable_collision"), TEXT("boolean"),
				TEXT("Enable or disable collision entirely"), false),
			FMCPToolParameter(TEXT("generate_overlap_events"), TEXT("boolean"),
				TEXT("Enable overlap events"), false)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	FMCPToolResult ExecuteGet(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteSetProfile(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteSetResponse(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteListProfiles(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteListChannels(const TSharedRef<FJsonObject>& Params);

	class UPrimitiveComponent* FindPrimitiveComponent(AActor* Actor, const FString& ComponentName, FString& OutError);
};
