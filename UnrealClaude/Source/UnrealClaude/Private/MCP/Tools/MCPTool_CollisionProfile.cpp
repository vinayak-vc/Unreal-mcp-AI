// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_CollisionProfile.h"
#include "UnrealClaudeModule.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/CollisionProfile.h"

FMCPToolResult FMCPTool_CollisionProfile::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	if (Operation == TEXT("get")) return ExecuteGet(Params);
	if (Operation == TEXT("set_profile")) return ExecuteSetProfile(Params);
	if (Operation == TEXT("set_response")) return ExecuteSetResponse(Params);
	if (Operation == TEXT("list_profiles")) return ExecuteListProfiles(Params);
	if (Operation == TEXT("list_channels")) return ExecuteListChannels(Params);

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: %s. Valid: get, set_profile, set_response, list_profiles, list_channels"),
		*Operation));
}

UPrimitiveComponent* FMCPTool_CollisionProfile::FindPrimitiveComponent(AActor* Actor, const FString& ComponentName, FString& OutError)
{
	if (!Actor)
	{
		OutError = TEXT("Null actor");
		return nullptr;
	}

	if (ComponentName.IsEmpty())
	{
		UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
		if (!Root)
		{
			TInlineComponentArray<UPrimitiveComponent*> Primitives;
			Actor->GetComponents(Primitives);
			if (Primitives.Num() > 0)
			{
				Root = Primitives[0];
			}
		}
		if (!Root)
		{
			OutError = FString::Printf(TEXT("Actor '%s' has no PrimitiveComponent"), *Actor->GetName());
		}
		return Root;
	}

	TInlineComponentArray<UPrimitiveComponent*> Primitives;
	Actor->GetComponents(Primitives);
	for (UPrimitiveComponent* Comp : Primitives)
	{
		if (Comp->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
		{
			return Comp;
		}
	}

	OutError = FString::Printf(TEXT("PrimitiveComponent '%s' not found on actor '%s'"), *ComponentName, *Actor->GetName());
	return nullptr;
}

FMCPToolResult FMCPTool_CollisionProfile::ExecuteGet(const TSharedRef<FJsonObject>& Params)
{
	UWorld* World = nullptr;
	if (auto Err = ValidateEditorContext(World)) return Err.GetValue();

	FString ActorName;
	TOptional<FMCPToolResult> Error;
	if (!ExtractActorName(Params, TEXT("actor_name"), ActorName, Error)) return Error.GetValue();

	AActor* Actor = FindActorByNameOrLabel(World, ActorName);
	if (!Actor) return ActorNotFoundError(ActorName);

	FString CompName = ExtractOptionalString(Params, TEXT("component_name"));
	FString CompError;
	UPrimitiveComponent* Comp = FindPrimitiveComponent(Actor, CompName, CompError);
	if (!Comp) return FMCPToolResult::Error(CompError);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("actor_name"), Actor->GetName());
	ResultData->SetStringField(TEXT("component_name"), Comp->GetName());
	ResultData->SetStringField(TEXT("collision_profile"), Comp->GetCollisionProfileName().ToString());
	ResultData->SetBoolField(TEXT("collision_enabled"), Comp->IsCollisionEnabled());
	ResultData->SetBoolField(TEXT("generate_overlap_events"), Comp->GetGenerateOverlapEvents());

	FString ObjectType;
	switch (Comp->GetCollisionObjectType())
	{
	case ECC_WorldStatic: ObjectType = TEXT("WorldStatic"); break;
	case ECC_WorldDynamic: ObjectType = TEXT("WorldDynamic"); break;
	case ECC_Pawn: ObjectType = TEXT("Pawn"); break;
	case ECC_PhysicsBody: ObjectType = TEXT("PhysicsBody"); break;
	case ECC_Vehicle: ObjectType = TEXT("Vehicle"); break;
	case ECC_Destructible: ObjectType = TEXT("Destructible"); break;
	default: ObjectType = FString::Printf(TEXT("Channel_%d"), static_cast<int32>(Comp->GetCollisionObjectType())); break;
	}
	ResultData->SetStringField(TEXT("object_type"), ObjectType);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Collision on '%s.%s': profile='%s'"),
			*Actor->GetName(), *Comp->GetName(), *Comp->GetCollisionProfileName().ToString()),
		ResultData);
}

FMCPToolResult FMCPTool_CollisionProfile::ExecuteSetProfile(const TSharedRef<FJsonObject>& Params)
{
	UWorld* World = nullptr;
	if (auto Err = ValidateEditorContext(World)) return Err.GetValue();

	FString ActorName, ProfileName;
	TOptional<FMCPToolResult> Error;
	if (!ExtractActorName(Params, TEXT("actor_name"), ActorName, Error)) return Error.GetValue();
	if (!ExtractRequiredString(Params, TEXT("profile_name"), ProfileName, Error)) return Error.GetValue();

	AActor* Actor = FindActorByNameOrLabel(World, ActorName);
	if (!Actor) return ActorNotFoundError(ActorName);

	FString CompName = ExtractOptionalString(Params, TEXT("component_name"));
	FString CompError;
	UPrimitiveComponent* Comp = FindPrimitiveComponent(Actor, CompName, CompError);
	if (!Comp) return FMCPToolResult::Error(CompError);

	Comp->SetCollisionProfileName(FName(*ProfileName));

	bool bEnableCollision;
	if (Params->TryGetBoolField(TEXT("enable_collision"), bEnableCollision))
	{
		Comp->SetCollisionEnabled(bEnableCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}

	bool bGenerateOverlap;
	if (Params->TryGetBoolField(TEXT("generate_overlap_events"), bGenerateOverlap))
	{
		Comp->SetGenerateOverlapEvents(bGenerateOverlap);
	}

	MarkActorDirty(Actor);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("profile_name"), ProfileName);
	ResultData->SetStringField(TEXT("actor_name"), Actor->GetName());
	ResultData->SetStringField(TEXT("component_name"), Comp->GetName());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Set collision profile '%s' on '%s.%s'"),
			*ProfileName, *Actor->GetName(), *Comp->GetName()),
		ResultData);
}

FMCPToolResult FMCPTool_CollisionProfile::ExecuteSetResponse(const TSharedRef<FJsonObject>& Params)
{
	UWorld* World = nullptr;
	if (auto Err = ValidateEditorContext(World)) return Err.GetValue();

	FString ActorName;
	TOptional<FMCPToolResult> Error;
	if (!ExtractActorName(Params, TEXT("actor_name"), ActorName, Error)) return Error.GetValue();

	AActor* Actor = FindActorByNameOrLabel(World, ActorName);
	if (!Actor) return ActorNotFoundError(ActorName);

	FString CompName = ExtractOptionalString(Params, TEXT("component_name"));
	FString CompError;
	UPrimitiveComponent* Comp = FindPrimitiveComponent(Actor, CompName, CompError);
	if (!Comp) return FMCPToolResult::Error(CompError);

	auto ParseResponse = [](const FString& Str) -> ECollisionResponse {
		if (Str.Equals(TEXT("Ignore"), ESearchCase::IgnoreCase)) return ECR_Ignore;
		if (Str.Equals(TEXT("Overlap"), ESearchCase::IgnoreCase)) return ECR_Overlap;
		return ECR_Block;
	};

	TArray<FString> ChangedChannels;

	auto ResolveChannel = [](const FString& ChannelName) -> ECollisionChannel {
		FName ChName(*ChannelName);
		int32 Index = UCollisionProfile::Get()->ReturnContainerIndexFromChannelName(ChName);
		return static_cast<ECollisionChannel>(Index);
	};

	// Single channel
	FString Channel = ExtractOptionalString(Params, TEXT("channel"));
	FString Response = ExtractOptionalString(Params, TEXT("response"));
	if (!Channel.IsEmpty() && !Response.IsEmpty())
	{
		Comp->SetCollisionResponseToChannel(ResolveChannel(Channel), ParseResponse(Response));
		ChangedChannels.Add(FString::Printf(TEXT("%s=%s"), *Channel, *Response));
	}

	// Batch responses
	const TSharedPtr<FJsonObject>* ResponsesObj;
	if (Params->TryGetObjectField(TEXT("responses"), ResponsesObj) && (*ResponsesObj).IsValid())
	{
		for (const auto& Pair : (*ResponsesObj)->Values)
		{
			FString RespStr;
			if (Pair.Value->TryGetString(RespStr))
			{
				Comp->SetCollisionResponseToChannel(ResolveChannel(Pair.Key), ParseResponse(RespStr));
				ChangedChannels.Add(FString::Printf(TEXT("%s=%s"), *Pair.Key, *RespStr));
			}
		}
	}

	if (ChangedChannels.Num() == 0)
	{
		return FMCPToolResult::Error(TEXT("No channel responses specified. Use 'channel'+'response' or 'responses' object."));
	}

	MarkActorDirty(Actor);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetArrayField(TEXT("changed"), StringArrayToJsonArray(ChangedChannels));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Set %d collision responses on '%s'"), ChangedChannels.Num(), *Actor->GetName()),
		ResultData);
}

FMCPToolResult FMCPTool_CollisionProfile::ExecuteListProfiles(const TSharedRef<FJsonObject>& Params)
{
	UCollisionProfile* Profile = UCollisionProfile::Get();
	if (!Profile)
	{
		return FMCPToolResult::Error(TEXT("CollisionProfile system not available"));
	}

	TArray<TSharedPtr<FJsonValue>> ProfileArray;

	// Standard collision profile preset names
	const TArray<FName> ProfileNames = {
		FName("NoCollision"), FName("BlockAll"), FName("OverlapAll"),
		FName("BlockAllDynamic"), FName("OverlapAllDynamic"), FName("IgnoreOnlyPawn"),
		FName("OverlapOnlyPawn"), FName("Pawn"), FName("Spectator"),
		FName("CharacterMesh"), FName("PhysicsActor"), FName("Destructible"),
		FName("InvisibleWall"), FName("InvisibleWallDynamic"), FName("Trigger"),
		FName("Ragdoll"), FName("Vehicle"), FName("UI")
	};

	for (const FName& PName : ProfileNames)
	{
		TSharedPtr<FJsonObject> PObj = MakeShared<FJsonObject>();
		PObj->SetStringField(TEXT("name"), PName.ToString());
		ProfileArray.Add(MakeShared<FJsonValueObject>(PObj));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetArrayField(TEXT("profiles"), ProfileArray);
	ResultData->SetNumberField(TEXT("count"), ProfileArray.Num());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Listed %d collision profiles"), ProfileArray.Num()),
		ResultData);
}

FMCPToolResult FMCPTool_CollisionProfile::ExecuteListChannels(const TSharedRef<FJsonObject>& Params)
{
	TArray<TSharedPtr<FJsonValue>> ChannelArray;

	struct FChannelInfo { const TCHAR* Name; ECollisionChannel Channel; };
	const FChannelInfo Channels[] = {
		{TEXT("WorldStatic"), ECC_WorldStatic},
		{TEXT("WorldDynamic"), ECC_WorldDynamic},
		{TEXT("Pawn"), ECC_Pawn},
		{TEXT("Visibility"), ECC_Visibility},
		{TEXT("Camera"), ECC_Camera},
		{TEXT("PhysicsBody"), ECC_PhysicsBody},
		{TEXT("Vehicle"), ECC_Vehicle},
		{TEXT("Destructible"), ECC_Destructible},
	};

	for (const auto& Ch : Channels)
	{
		TSharedPtr<FJsonObject> ChObj = MakeShared<FJsonObject>();
		ChObj->SetStringField(TEXT("name"), Ch.Name);
		ChObj->SetNumberField(TEXT("index"), static_cast<int32>(Ch.Channel));
		ChannelArray.Add(MakeShared<FJsonValueObject>(ChObj));
	}

	// Custom channels (ECC_GameTraceChannel1 through 18)
	UCollisionProfile* Profile = UCollisionProfile::Get();
	if (Profile)
	{
		for (int32 i = 0; i < ECC_GameTraceChannel18 - ECC_GameTraceChannel1 + 1; ++i)
		{
			ECollisionChannel Channel = static_cast<ECollisionChannel>(ECC_GameTraceChannel1 + i);
			FName ChannelName = Profile->ReturnChannelNameFromContainerIndex(static_cast<int32>(ECC_GameTraceChannel1) + i);
			if (ChannelName != NAME_None && !ChannelName.ToString().StartsWith(TEXT("ECC_")))
			{
				TSharedPtr<FJsonObject> ChObj = MakeShared<FJsonObject>();
				ChObj->SetStringField(TEXT("name"), ChannelName.ToString());
				ChObj->SetNumberField(TEXT("index"), static_cast<int32>(Channel));
				ChObj->SetBoolField(TEXT("custom"), true);
				ChannelArray.Add(MakeShared<FJsonValueObject>(ChObj));
			}
		}
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetArrayField(TEXT("channels"), ChannelArray);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Listed %d collision channels"), ChannelArray.Num()),
		ResultData);
}
