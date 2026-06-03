// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_Niagara.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"
#include "UnrealClaudeUtils.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

FMCPToolResult FMCPTool_Niagara::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	const FString OpLower = Operation.ToLower();

	if (OpLower == TEXT("spawn_system"))   return ExecuteSpawnSystem(Params);
	if (OpLower == TEXT("set_parameter"))  return ExecuteSetParameter(Params);
	if (OpLower == TEXT("list_systems"))   return ExecuteListSystems(Params);

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: '%s'. Valid: spawn_system, set_parameter, list_systems"), *Operation));
}

FMCPToolResult FMCPTool_Niagara::ExecuteSpawnSystem(const TSharedRef<FJsonObject>& Params)
{
	UWorld* World = nullptr;
	if (auto Err = ValidateEditorContext(World)) return Err.GetValue();

	FString SystemPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("system_path"), SystemPath, Error))
	{
		return Error.GetValue();
	}

	// Load the NiagaraSystem asset
	FString AdjustedPath = SystemPath;
	if (!AdjustedPath.Contains(TEXT(".")))
	{
		AdjustedPath += TEXT(".") + FPaths::GetBaseFilename(SystemPath);
	}

	UNiagaraSystem* NiagaraSystem = LoadObject<UNiagaraSystem>(nullptr, *AdjustedPath);
	if (!NiagaraSystem)
	{
		NiagaraSystem = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	}
	if (!NiagaraSystem)
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Failed to load NiagaraSystem: '%s'. Use 'niagara list_systems' to find available assets."),
			*SystemPath));
	}

	const FVector  Location = ExtractVectorParam(Params,   TEXT("location"));
	const FRotator Rotation = ExtractRotatorParam(Params,  TEXT("rotation"));
	const FVector  Scale    = ExtractScaleParam(Params,    TEXT("scale"));
	const bool     bAutoDestroy = ExtractOptionalBool(Params, TEXT("auto_destroy"), true);

	UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		NiagaraSystem,
		Location,
		Rotation,
		Scale,
		bAutoDestroy,
		/*bAutoActivate=*/true,
		ENCPoolMethod::None
	);

	if (!NiagaraComp)
	{
		return FMCPToolResult::Error(TEXT("Failed to spawn NiagaraSystem — check PIE is not running"));
	}

	// Optional actor label
	FString ActorLabel;
	Params->TryGetStringField(TEXT("actor_name"), ActorLabel);
	if (!ActorLabel.IsEmpty())
	{
		if (AActor* OwnerActor = NiagaraComp->GetOwner())
		{
			OwnerActor->SetActorLabel(ActorLabel);
		}
	}

	MarkWorldDirty(World);

	AActor* OwnerActor = NiagaraComp->GetOwner();
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("system"),       NiagaraSystem->GetName());
	ResultData->SetStringField(TEXT("system_path"),  SystemPath);
	ResultData->SetStringField(TEXT("actor"),        OwnerActor ? OwnerActor->GetName() : TEXT("None"));
	ResultData->SetStringField(TEXT("actor_label"),  OwnerActor ? OwnerActor->GetActorLabel() : TEXT("None"));
	ResultData->SetObjectField(TEXT("location"),     UnrealClaudeJsonUtils::VectorToJson(Location));
	ResultData->SetBoolField(TEXT("auto_destroy"),   bAutoDestroy);
	ResultData->SetStringField(TEXT("tip"),
		TEXT("Use 'niagara set_parameter' with actor_name to modify parameters at runtime"));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Spawned NiagaraSystem '%s'"), *NiagaraSystem->GetName()),
		ResultData);
}

FMCPToolResult FMCPTool_Niagara::ExecuteSetParameter(const TSharedRef<FJsonObject>& Params)
{
	UWorld* World = nullptr;
	if (auto Err = ValidateEditorContext(World)) return Err.GetValue();

	FString ActorName;
	TOptional<FMCPToolResult> Error;
	if (!ExtractActorName(Params, TEXT("actor_name"), ActorName, Error))
	{
		return Error.GetValue();
	}

	FString ParamName;
	if (!ExtractRequiredString(Params, TEXT("param_name"), ParamName, Error))
	{
		return Error.GetValue();
	}

	FString ParamType = ExtractOptionalString(Params, TEXT("param_type"), TEXT("float")).ToLower();

	AActor* Actor = FindActorByNameOrLabel(World, ActorName);
	if (!Actor) return ActorNotFoundError(ActorName);

	UNiagaraComponent* NiagaraComp = Actor->FindComponentByClass<UNiagaraComponent>();
	if (!NiagaraComp)
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Actor '%s' has no NiagaraComponent"), *ActorName));
	}

	const TSharedPtr<FJsonValue> ValueField = Params->TryGetField(TEXT("param_value"));
	if (!ValueField.IsValid())
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: param_value"));
	}

	if (ParamType == TEXT("float"))
	{
		double Val = 0.0;
		ValueField->TryGetNumber(Val);
		NiagaraComp->SetVariableFloat(FName(*ParamName), static_cast<float>(Val));
	}
	else if (ParamType == TEXT("int"))
	{
		int64 Val = 0;
		ValueField->TryGetNumber(Val);
		NiagaraComp->SetVariableInt(FName(*ParamName), static_cast<int32>(Val));
	}
	else if (ParamType == TEXT("bool"))
	{
		bool Val = false;
		ValueField->TryGetBool(Val);
		NiagaraComp->SetVariableBool(FName(*ParamName), Val);
	}
	else if (ParamType == TEXT("vector"))
	{
		FVector Vec;
		const TSharedPtr<FJsonObject>* ObjVal;
		if (ValueField->TryGetObject(ObjVal))
		{
			(*ObjVal)->TryGetNumberField(TEXT("x"), Vec.X);
			(*ObjVal)->TryGetNumberField(TEXT("y"), Vec.Y);
			(*ObjVal)->TryGetNumberField(TEXT("z"), Vec.Z);
		}
		NiagaraComp->SetVariableVec3(FName(*ParamName), Vec);
	}
	else if (ParamType == TEXT("color"))
	{
		FLinearColor Color = FLinearColor::White;
		const TSharedPtr<FJsonObject>* ObjVal;
		if (ValueField->TryGetObject(ObjVal))
		{
			(*ObjVal)->TryGetNumberField(TEXT("r"), Color.R);
			(*ObjVal)->TryGetNumberField(TEXT("g"), Color.G);
			(*ObjVal)->TryGetNumberField(TEXT("b"), Color.B);
			(*ObjVal)->TryGetNumberField(TEXT("a"), Color.A);
		}
		NiagaraComp->SetVariableLinearColor(FName(*ParamName), Color);
	}
	else
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Unknown param_type '%s'. Valid: float, int, bool, vector, color"), *ParamType));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("actor"),      ActorName);
	ResultData->SetStringField(TEXT("param_name"), ParamName);
	ResultData->SetStringField(TEXT("param_type"), ParamType);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Set Niagara parameter '%s' on '%s'"), *ParamName, *ActorName),
		ResultData);
}

FMCPToolResult FMCPTool_Niagara::ExecuteListSystems(const TSharedRef<FJsonObject>& Params)
{
	const FString PackagePath = ExtractOptionalString(Params, TEXT("package_path"), TEXT("/Game/"));
	const FString NameFilter  = ExtractOptionalString(Params, TEXT("name_filter"));
	const int32   Limit       = FMath::Clamp(ExtractOptionalNumber<int32>(Params, TEXT("limit"), 50), 1, 500);

	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	TArray<FAssetData> AllAssets;
	AssetRegistry.GetAssetsByClass(UNiagaraSystem::StaticClass()->GetClassPathName(), AllAssets, /*bSearchSubClasses=*/true);

	TArray<TSharedPtr<FJsonValue>> Results;
	int32 TotalFound = 0;

	for (const FAssetData& AssetData : AllAssets)
	{
		const FString PkgName = AssetData.PackageName.ToString();
		if (!PkgName.StartsWith(PackagePath)) continue;
		if (!NameFilter.IsEmpty() && !AssetData.AssetName.ToString().Contains(NameFilter, ESearchCase::IgnoreCase)) continue;

		TotalFound++;
		if (Results.Num() < Limit)
		{
			TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
			Obj->SetStringField(TEXT("name"),         AssetData.AssetName.ToString());
			Obj->SetStringField(TEXT("path"),         AssetData.GetObjectPathString());
			Obj->SetStringField(TEXT("package"),      PkgName);
			Results.Add(MakeShared<FJsonValueObject>(Obj));
		}
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("package_path"),  PackagePath);
	ResultData->SetNumberField(TEXT("count"),         Results.Num());
	ResultData->SetNumberField(TEXT("total_found"),   TotalFound);
	ResultData->SetArrayField(TEXT("systems"),        Results);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Found %d NiagaraSystem asset(s) under '%s'"), TotalFound, *PackagePath),
		ResultData);
}
