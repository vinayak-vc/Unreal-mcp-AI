// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_Curve.h"
#include "UnrealClaudeModule.h"
#include "MCP/MCPParamValidator.h"

#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveLinearColor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"

FMCPToolResult FMCPTool_Curve::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	if (Operation == TEXT("create")) return ExecuteCreate(Params);
	if (Operation == TEXT("add_key")) return ExecuteAddKey(Params);
	if (Operation == TEXT("get_keys")) return ExecuteGetKeys(Params);
	if (Operation == TEXT("evaluate")) return ExecuteEvaluate(Params);
	if (Operation == TEXT("set_keys")) return ExecuteSetKeys(Params);

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: %s. Valid: create, add_key, get_keys, evaluate, set_keys"),
		*Operation));
}

FMCPToolResult FMCPTool_Curve::ExecuteCreate(const TSharedRef<FJsonObject>& Params)
{
	FString CurveName;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("curve_name"), CurveName, Error))
	{
		return Error.GetValue();
	}

	FString CurveType = ExtractOptionalString(Params, TEXT("curve_type"), TEXT("Float"));
	FString PackagePath = ExtractOptionalString(Params, TEXT("package_path"), TEXT("/Game/Curves/"));
	FString FullPath = PackagePath / CurveName;

	UPackage* Package = CreatePackage(*FullPath);
	if (!Package)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to create package: %s"), *FullPath));
	}

	UObject* NewCurve = nullptr;
	if (CurveType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
	{
		NewCurve = NewObject<UCurveFloat>(Package, FName(*CurveName), RF_Public | RF_Standalone);
	}
	else if (CurveType.Equals(TEXT("Vector"), ESearchCase::IgnoreCase))
	{
		NewCurve = NewObject<UCurveVector>(Package, FName(*CurveName), RF_Public | RF_Standalone);
	}
	else if (CurveType.Equals(TEXT("LinearColor"), ESearchCase::IgnoreCase))
	{
		NewCurve = NewObject<UCurveLinearColor>(Package, FName(*CurveName), RF_Public | RF_Standalone);
	}
	else
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Invalid curve_type: %s. Valid: Float, Vector, LinearColor"), *CurveType));
	}

	if (!NewCurve)
	{
		return FMCPToolResult::Error(TEXT("Failed to create curve asset"));
	}

	Package->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(FullPath, false);
	FAssetRegistryModule::AssetCreated(NewCurve);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("asset_path"), NewCurve->GetPathName());
	ResultData->SetStringField(TEXT("name"), CurveName);
	ResultData->SetStringField(TEXT("type"), CurveType);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Created %s curve '%s'"), *CurveType, *CurveName), ResultData);
}

FMCPToolResult FMCPTool_Curve::ExecuteAddKey(const TSharedRef<FJsonObject>& Params)
{
	FString CurvePath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("curve_path"), CurvePath, Error))
	{
		return Error.GetValue();
	}

	double Time;
	if (!Params->TryGetNumberField(TEXT("time"), Time))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: time"));
	}

	FString AdjustedPath = CurvePath;
	if (!AdjustedPath.Contains(TEXT(".")))
	{
		AdjustedPath += TEXT(".") + FPaths::GetBaseFilename(CurvePath);
	}

	UObject* Asset = LoadObject<UObject>(nullptr, *AdjustedPath);
	if (!Asset) Asset = LoadObject<UObject>(nullptr, *CurvePath);
	if (!Asset) return FMCPToolResult::Error(FString::Printf(TEXT("Curve not found: %s"), *CurvePath));

	FString InterpStr = ExtractOptionalString(Params, TEXT("interp_mode"), TEXT("Cubic"));
	ERichCurveInterpMode InterpMode = RCIM_Cubic;
	if (InterpStr.Equals(TEXT("Linear"), ESearchCase::IgnoreCase)) InterpMode = RCIM_Linear;
	else if (InterpStr.Equals(TEXT("Constant"), ESearchCase::IgnoreCase)) InterpMode = RCIM_Constant;

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetNumberField(TEXT("time"), Time);

	if (UCurveFloat* FloatCurve = Cast<UCurveFloat>(Asset))
	{
		double Value = ExtractOptionalNumber<double>(Params, TEXT("value"), 0.0);
		FKeyHandle Handle = FloatCurve->FloatCurve.AddKey(static_cast<float>(Time), static_cast<float>(Value));
		FloatCurve->FloatCurve.SetKeyInterpMode(Handle, InterpMode);
		ResultData->SetNumberField(TEXT("value"), Value);
	}
	else if (UCurveVector* VecCurve = Cast<UCurveVector>(Asset))
	{
		FVector Vec = ExtractVectorParam(Params, TEXT("vector_value"), FVector::ZeroVector);
		VecCurve->FloatCurves[0].AddKey(static_cast<float>(Time), static_cast<float>(Vec.X));
		VecCurve->FloatCurves[1].AddKey(static_cast<float>(Time), static_cast<float>(Vec.Y));
		VecCurve->FloatCurves[2].AddKey(static_cast<float>(Time), static_cast<float>(Vec.Z));
		ResultData->SetObjectField(TEXT("value"), UnrealClaudeJsonUtils::VectorToJson(Vec));
	}
	else if (UCurveLinearColor* ColorCurve = Cast<UCurveLinearColor>(Asset))
	{
		const TSharedPtr<FJsonObject>* ColorObj;
		FLinearColor Color(0, 0, 0, 1);
		if (Params->TryGetObjectField(TEXT("color_value"), ColorObj) && (*ColorObj).IsValid())
		{
			double R = 0, G = 0, B = 0, A = 1;
			(*ColorObj)->TryGetNumberField(TEXT("r"), R);
			(*ColorObj)->TryGetNumberField(TEXT("g"), G);
			(*ColorObj)->TryGetNumberField(TEXT("b"), B);
			(*ColorObj)->TryGetNumberField(TEXT("a"), A);
			Color = FLinearColor(static_cast<float>(R), static_cast<float>(G), static_cast<float>(B), static_cast<float>(A));
		}
		ColorCurve->FloatCurves[0].AddKey(static_cast<float>(Time), Color.R);
		ColorCurve->FloatCurves[1].AddKey(static_cast<float>(Time), Color.G);
		ColorCurve->FloatCurves[2].AddKey(static_cast<float>(Time), Color.B);
		ColorCurve->FloatCurves[3].AddKey(static_cast<float>(Time), Color.A);
	}
	else
	{
		return FMCPToolResult::Error(TEXT("Asset is not a supported curve type"));
	}

	Asset->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(Asset->GetOutermost()->GetPathName(), false);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added key at time %.3f"), Time), ResultData);
}

FMCPToolResult FMCPTool_Curve::ExecuteGetKeys(const TSharedRef<FJsonObject>& Params)
{
	FString CurvePath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("curve_path"), CurvePath, Error))
	{
		return Error.GetValue();
	}

	FString AdjustedPath = CurvePath;
	if (!AdjustedPath.Contains(TEXT(".")))
	{
		AdjustedPath += TEXT(".") + FPaths::GetBaseFilename(CurvePath);
	}

	UObject* Asset = LoadObject<UObject>(nullptr, *AdjustedPath);
	if (!Asset) Asset = LoadObject<UObject>(nullptr, *CurvePath);
	if (!Asset) return FMCPToolResult::Error(FString::Printf(TEXT("Curve not found: %s"), *CurvePath));

	TArray<TSharedPtr<FJsonValue>> KeysArray;
	FString CurveType;

	if (UCurveFloat* FloatCurve = Cast<UCurveFloat>(Asset))
	{
		CurveType = TEXT("Float");
		for (const FRichCurveKey& Key : FloatCurve->FloatCurve.GetConstRefOfKeys())
		{
			TSharedPtr<FJsonObject> KeyObj = MakeShared<FJsonObject>();
			KeyObj->SetNumberField(TEXT("time"), Key.Time);
			KeyObj->SetNumberField(TEXT("value"), Key.Value);
			KeysArray.Add(MakeShared<FJsonValueObject>(KeyObj));
		}
	}
	else if (UCurveVector* VecCurve = Cast<UCurveVector>(Asset))
	{
		CurveType = TEXT("Vector");
		const auto& XKeys = VecCurve->FloatCurves[0].GetConstRefOfKeys();
		for (int32 i = 0; i < XKeys.Num(); ++i)
		{
			TSharedPtr<FJsonObject> KeyObj = MakeShared<FJsonObject>();
			KeyObj->SetNumberField(TEXT("time"), XKeys[i].Time);
			KeyObj->SetNumberField(TEXT("x"), XKeys[i].Value);
			if (i < VecCurve->FloatCurves[1].GetConstRefOfKeys().Num())
				KeyObj->SetNumberField(TEXT("y"), VecCurve->FloatCurves[1].GetConstRefOfKeys()[i].Value);
			if (i < VecCurve->FloatCurves[2].GetConstRefOfKeys().Num())
				KeyObj->SetNumberField(TEXT("z"), VecCurve->FloatCurves[2].GetConstRefOfKeys()[i].Value);
			KeysArray.Add(MakeShared<FJsonValueObject>(KeyObj));
		}
	}
	else if (UCurveLinearColor* ColorCurve = Cast<UCurveLinearColor>(Asset))
	{
		CurveType = TEXT("LinearColor");
		const auto& RKeys = ColorCurve->FloatCurves[0].GetConstRefOfKeys();
		for (int32 i = 0; i < RKeys.Num(); ++i)
		{
			TSharedPtr<FJsonObject> KeyObj = MakeShared<FJsonObject>();
			KeyObj->SetNumberField(TEXT("time"), RKeys[i].Time);
			KeyObj->SetNumberField(TEXT("r"), RKeys[i].Value);
			if (i < ColorCurve->FloatCurves[1].GetConstRefOfKeys().Num())
				KeyObj->SetNumberField(TEXT("g"), ColorCurve->FloatCurves[1].GetConstRefOfKeys()[i].Value);
			if (i < ColorCurve->FloatCurves[2].GetConstRefOfKeys().Num())
				KeyObj->SetNumberField(TEXT("b"), ColorCurve->FloatCurves[2].GetConstRefOfKeys()[i].Value);
			if (i < ColorCurve->FloatCurves[3].GetConstRefOfKeys().Num())
				KeyObj->SetNumberField(TEXT("a"), ColorCurve->FloatCurves[3].GetConstRefOfKeys()[i].Value);
			KeysArray.Add(MakeShared<FJsonValueObject>(KeyObj));
		}
	}
	else
	{
		return FMCPToolResult::Error(TEXT("Asset is not a supported curve type"));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("curve_type"), CurveType);
	ResultData->SetNumberField(TEXT("key_count"), KeysArray.Num());
	ResultData->SetArrayField(TEXT("keys"), KeysArray);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Curve '%s' has %d keys"), *Asset->GetName(), KeysArray.Num()),
		ResultData);
}

FMCPToolResult FMCPTool_Curve::ExecuteEvaluate(const TSharedRef<FJsonObject>& Params)
{
	FString CurvePath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("curve_path"), CurvePath, Error)) return Error.GetValue();

	double Time;
	if (!Params->TryGetNumberField(TEXT("time"), Time))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: time"));
	}

	FString AdjustedPath = CurvePath;
	if (!AdjustedPath.Contains(TEXT(".")))
	{
		AdjustedPath += TEXT(".") + FPaths::GetBaseFilename(CurvePath);
	}

	UObject* Asset = LoadObject<UObject>(nullptr, *AdjustedPath);
	if (!Asset) Asset = LoadObject<UObject>(nullptr, *CurvePath);
	if (!Asset) return FMCPToolResult::Error(FString::Printf(TEXT("Curve not found: %s"), *CurvePath));

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetNumberField(TEXT("time"), Time);

	if (UCurveFloat* FloatCurve = Cast<UCurveFloat>(Asset))
	{
		float Val = FloatCurve->GetFloatValue(static_cast<float>(Time));
		ResultData->SetNumberField(TEXT("value"), Val);
	}
	else if (UCurveVector* VecCurve = Cast<UCurveVector>(Asset))
	{
		FVector Val = VecCurve->GetVectorValue(static_cast<float>(Time));
		ResultData->SetObjectField(TEXT("value"), UnrealClaudeJsonUtils::VectorToJson(Val));
	}
	else if (UCurveLinearColor* ColorCurve = Cast<UCurveLinearColor>(Asset))
	{
		FLinearColor Val = ColorCurve->GetLinearColorValue(static_cast<float>(Time));
		TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
		ColorObj->SetNumberField(TEXT("r"), Val.R);
		ColorObj->SetNumberField(TEXT("g"), Val.G);
		ColorObj->SetNumberField(TEXT("b"), Val.B);
		ColorObj->SetNumberField(TEXT("a"), Val.A);
		ResultData->SetObjectField(TEXT("value"), ColorObj);
	}

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Evaluated '%s' at time %.3f"), *Asset->GetName(), Time),
		ResultData);
}

FMCPToolResult FMCPTool_Curve::ExecuteSetKeys(const TSharedRef<FJsonObject>& Params)
{
	FString CurvePath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("curve_path"), CurvePath, Error)) return Error.GetValue();

	const TArray<TSharedPtr<FJsonValue>>* KeysArray;
	if (!Params->TryGetArrayField(TEXT("keys"), KeysArray))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: keys (array of key objects)"));
	}

	FString AdjustedPath = CurvePath;
	if (!AdjustedPath.Contains(TEXT(".")))
	{
		AdjustedPath += TEXT(".") + FPaths::GetBaseFilename(CurvePath);
	}

	UObject* Asset = LoadObject<UObject>(nullptr, *AdjustedPath);
	if (!Asset) Asset = LoadObject<UObject>(nullptr, *CurvePath);
	if (!Asset) return FMCPToolResult::Error(FString::Printf(TEXT("Curve not found: %s"), *CurvePath));

	int32 KeyCount = 0;

	if (UCurveFloat* FloatCurve = Cast<UCurveFloat>(Asset))
	{
		FloatCurve->FloatCurve.Reset();
		for (const auto& KeyVal : *KeysArray)
		{
			const TSharedPtr<FJsonObject>* KeyObj;
			if (KeyVal->TryGetObject(KeyObj))
			{
				double T, V;
				if ((*KeyObj)->TryGetNumberField(TEXT("time"), T) && (*KeyObj)->TryGetNumberField(TEXT("value"), V))
				{
					FloatCurve->FloatCurve.AddKey(static_cast<float>(T), static_cast<float>(V));
					KeyCount++;
				}
			}
		}
	}
	else if (UCurveVector* VecCurve = Cast<UCurveVector>(Asset))
	{
		for (int i = 0; i < 3; ++i) VecCurve->FloatCurves[i].Reset();
		for (const auto& KeyVal : *KeysArray)
		{
			const TSharedPtr<FJsonObject>* KeyObj;
			if (KeyVal->TryGetObject(KeyObj))
			{
				double T, X, Y, Z;
				if ((*KeyObj)->TryGetNumberField(TEXT("time"), T))
				{
					(*KeyObj)->TryGetNumberField(TEXT("x"), X);
					(*KeyObj)->TryGetNumberField(TEXT("y"), Y);
					(*KeyObj)->TryGetNumberField(TEXT("z"), Z);
					VecCurve->FloatCurves[0].AddKey(static_cast<float>(T), static_cast<float>(X));
					VecCurve->FloatCurves[1].AddKey(static_cast<float>(T), static_cast<float>(Y));
					VecCurve->FloatCurves[2].AddKey(static_cast<float>(T), static_cast<float>(Z));
					KeyCount++;
				}
			}
		}
	}

	Asset->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(Asset->GetOutermost()->GetPathName(), false);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetNumberField(TEXT("keys_set"), KeyCount);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Set %d keys on '%s'"), KeyCount, *Asset->GetName()), ResultData);
}
