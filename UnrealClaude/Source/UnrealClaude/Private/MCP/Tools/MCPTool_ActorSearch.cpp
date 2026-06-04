// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_ActorSearch.h"
#include "UnrealClaudeModule.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

FMCPToolResult FMCPTool_ActorSearch::Execute(const TSharedRef<FJsonObject>& Params)
{
	UWorld* World = nullptr;
	if (auto Err = ValidateEditorContext(World))
	{
		return Err.GetValue();
	}

	// Extract filters
	FString ClassFilter = ExtractOptionalString(Params, TEXT("class_name"));
	FString NamePattern = ExtractOptionalString(Params, TEXT("name_pattern"));
	FString SingleTag = ExtractOptionalString(Params, TEXT("tag"));
	FString ComponentFilter = ExtractOptionalString(Params, TEXT("has_component"));
	FString PropName = ExtractOptionalString(Params, TEXT("property_name"));
	FString PropValue = ExtractOptionalString(Params, TEXT("property_value"));
	bool bIncludeHidden = ExtractOptionalBool(Params, TEXT("include_hidden"), false);
	int32 Limit = FMath::Clamp(ExtractOptionalNumber<int32>(Params, TEXT("limit"), 50), 1, 500);

	// Tags array
	TArray<FString> RequiredTags;
	if (!SingleTag.IsEmpty())
	{
		RequiredTags.Add(SingleTag);
	}
	const TArray<TSharedPtr<FJsonValue>>* TagsArray;
	if (Params->TryGetArrayField(TEXT("tags"), TagsArray))
	{
		for (const auto& TagVal : *TagsArray)
		{
			FString T;
			if (TagVal->TryGetString(T))
			{
				RequiredTags.AddUnique(T);
			}
		}
	}

	// Spatial filter
	bool bSpatialFilter = false;
	FVector SearchCenter = FVector::ZeroVector;
	float SearchRadius = 1000.0f;
	if (HasVectorParam(Params, TEXT("near_location")))
	{
		SearchCenter = ExtractVectorParam(Params, TEXT("near_location"));
		SearchRadius = ExtractOptionalNumber<float>(Params, TEXT("radius"), 1000.0f);
		bSpatialFilter = true;
	}

	// Verify at least one filter
	if (ClassFilter.IsEmpty() && NamePattern.IsEmpty() && RequiredTags.Num() == 0 &&
		ComponentFilter.IsEmpty() && PropName.IsEmpty() && !bSpatialFilter)
	{
		return FMCPToolResult::Error(TEXT("At least one filter required: class_name, name_pattern, tag, tags, has_component, property_name+property_value, or near_location"));
	}

	TArray<TSharedPtr<FJsonValue>> ResultActors;
	int32 TotalMatched = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		if (!bIncludeHidden && Actor->IsHidden()) continue;

		// Class filter
		if (!ClassFilter.IsEmpty())
		{
			FString ActorClassName = Actor->GetClass()->GetName();
			bool bClassMatch = ActorClassName.Equals(ClassFilter, ESearchCase::IgnoreCase);
			if (!bClassMatch)
			{
				for (UClass* C = Actor->GetClass()->GetSuperClass(); C; C = C->GetSuperClass())
				{
					if (C->GetName().Equals(ClassFilter, ESearchCase::IgnoreCase))
					{
						bClassMatch = true;
						break;
					}
				}
			}
			if (!bClassMatch) continue;
		}

		// Name pattern
		if (!NamePattern.IsEmpty())
		{
			bool bNameMatch = Actor->GetName().Contains(NamePattern, ESearchCase::IgnoreCase)
				|| Actor->GetActorLabel().Contains(NamePattern, ESearchCase::IgnoreCase);
			if (!bNameMatch) continue;
		}

		// Tags
		if (RequiredTags.Num() > 0)
		{
			bool bAllTags = true;
			for (const FString& Tag : RequiredTags)
			{
				if (!Actor->Tags.ContainsByPredicate([&Tag](const FName& N) {
					return N.ToString().Equals(Tag, ESearchCase::IgnoreCase);
				}))
				{
					bAllTags = false;
					break;
				}
			}
			if (!bAllTags) continue;
		}

		// Component filter
		if (!ComponentFilter.IsEmpty())
		{
			bool bHasComp = false;
			TInlineComponentArray<UActorComponent*> Components;
			Actor->GetComponents(Components);
			for (UActorComponent* Comp : Components)
			{
				if (Comp && Comp->GetClass()->GetName().Contains(ComponentFilter, ESearchCase::IgnoreCase))
				{
					bHasComp = true;
					break;
				}
			}
			if (!bHasComp) continue;
		}

		// Property filter
		if (!PropName.IsEmpty())
		{
			FProperty* Prop = FindFProperty<FProperty>(Actor->GetClass(), FName(*PropName));
			if (!Prop) continue;

			FString ActualValue;
			const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Actor);
			Prop->ExportTextItem_Direct(ActualValue, ValuePtr, nullptr, Actor, PPF_None);

			if (!ActualValue.Equals(PropValue, ESearchCase::IgnoreCase)) continue;
		}

		// Spatial filter
		if (bSpatialFilter)
		{
			float Dist = FVector::Dist(Actor->GetActorLocation(), SearchCenter);
			if (Dist > SearchRadius) continue;
		}

		TotalMatched++;
		if (ResultActors.Num() >= Limit) continue;

		TSharedPtr<FJsonObject> ActorJson = BuildActorInfoWithTransformJson(Actor);

		// Add tags
		if (Actor->Tags.Num() > 0)
		{
			TArray<FString> TagStrs;
			for (const FName& T : Actor->Tags)
			{
				TagStrs.Add(T.ToString());
			}
			ActorJson->SetArrayField(TEXT("tags"), StringArrayToJsonArray(TagStrs));
		}

		// Add distance if spatial query
		if (bSpatialFilter)
		{
			ActorJson->SetNumberField(TEXT("distance"),
				FVector::Dist(Actor->GetActorLocation(), SearchCenter));
		}

		ResultActors.Add(MakeShared<FJsonValueObject>(ActorJson));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetNumberField(TEXT("count"), ResultActors.Num());
	ResultData->SetNumberField(TEXT("total_matched"), TotalMatched);
	ResultData->SetArrayField(TEXT("actors"), ResultActors);
	if (TotalMatched > Limit)
	{
		ResultData->SetBoolField(TEXT("truncated"), true);
	}

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Found %d actors (showing %d)"), TotalMatched, ResultActors.Num()),
		ResultData);
}
