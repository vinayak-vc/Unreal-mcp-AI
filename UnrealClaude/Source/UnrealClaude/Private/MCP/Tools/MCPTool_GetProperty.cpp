// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_GetProperty.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"

FMCPToolResult FMCPTool_GetProperty::Execute(const TSharedRef<FJsonObject>& Params)
{
	UWorld* World = nullptr;
	if (auto Error = ValidateEditorContext(World))
	{
		return Error.GetValue();
	}

	FString ActorName;
	TOptional<FMCPToolResult> ParamError;
	if (!ExtractActorName(Params, TEXT("actor_name"), ActorName, ParamError))
	{
		return ParamError.GetValue();
	}

	FString PropertyPath;
	if (!ExtractRequiredString(Params, TEXT("property"), PropertyPath, ParamError))
	{
		return ParamError.GetValue();
	}
	if (!ValidatePropertyPathParam(PropertyPath, ParamError))
	{
		return ParamError.GetValue();
	}

	AActor* Actor = FindActorByNameOrLabel(World, ActorName);
	if (!Actor)
	{
		return ActorNotFoundError(ActorName);
	}

	FString ErrorMessage;
	TSharedPtr<FJsonValue> Value = ReadPropertyToJson(Actor, PropertyPath, ErrorMessage);
	if (!Value.IsValid())
	{
		return FMCPToolResult::Error(ErrorMessage);
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("actor"), Actor->GetName());
	ResultData->SetStringField(TEXT("property"), PropertyPath);
	ResultData->SetField(TEXT("value"), Value);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Read property '%s' from actor '%s'"), *PropertyPath, *Actor->GetName()),
		ResultData);
}

TSharedPtr<FJsonValue> FMCPTool_GetProperty::ReadPropertyToJson(
	UObject* Object,
	const FString& PropertyPath,
	FString& OutError)
{
	if (!Object)
	{
		OutError = TEXT("Object is null");
		return nullptr;
	}

	TArray<FString> PathParts;
	PropertyPath.ParseIntoArray(PathParts, TEXT("."), true);

	UObject* CurrentObject = Object;

	for (int32 i = 0; i < PathParts.Num(); ++i)
	{
		const FString& PartName = PathParts[i];
		const bool bIsLastPart = (i == PathParts.Num() - 1);

		FProperty* Prop = CurrentObject->GetClass()->FindPropertyByName(FName(*PartName));

		if (!Prop)
		{
			// Try component navigation on actors
			if (AActor* Actor = Cast<AActor>(CurrentObject))
			{
				bool bFoundComp = false;
				for (UActorComponent* Comp : Actor->GetComponents())
				{
					if (Comp && Comp->GetName().Contains(PartName))
					{
						if (bIsLastPart)
						{
							OutError = FString::Printf(
								TEXT("Cannot read component '%s' directly. Use a path like '%s.PropertyName'."),
								*PartName, *Comp->GetName());
							return nullptr;
						}
						CurrentObject = Comp;
						bFoundComp = true;
						break;
					}
				}
				if (bFoundComp) continue;
			}

			// Build helpful error
			TArray<FString> Available;
			for (TFieldIterator<FProperty> It(CurrentObject->GetClass()); It && Available.Num() < 20; ++It)
			{
				Available.Add(It->GetName());
			}
			OutError = FString::Printf(TEXT("Property '%s' not found on %s. Available: %s"),
				*PartName, *CurrentObject->GetClass()->GetName(), *FString::Join(Available, TEXT(", ")));
			return nullptr;
		}

		if (!bIsLastPart)
		{
			// Navigate into nested object
			FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop);
			if (!ObjProp)
			{
				OutError = FString::Printf(TEXT("Cannot navigate into non-object property: %s"), *PartName);
				return nullptr;
			}
			UObject* Nested = ObjProp->GetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(CurrentObject));
			if (!Nested)
			{
				OutError = FString::Printf(TEXT("Nested object is null at: %s"), *PartName);
				return nullptr;
			}
			CurrentObject = Nested;
			continue;
		}

		// Last part — read value
		const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(CurrentObject);
		return PropertyValueToJson(Prop, ValuePtr);
	}

	OutError = TEXT("Empty property path");
	return nullptr;
}

TSharedPtr<FJsonValue> FMCPTool_GetProperty::PropertyValueToJson(FProperty* Property, const void* ValuePtr)
{
	if (!Property || !ValuePtr) return MakeShared<FJsonValueNull>();

	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		return MakeShared<FJsonValueBoolean>(BoolProp->GetPropertyValue(ValuePtr));
	}
	if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		if (NumProp->IsFloatingPoint())
			return MakeShared<FJsonValueNumber>(NumProp->GetFloatingPointPropertyValue(ValuePtr));
		if (NumProp->IsInteger())
			return MakeShared<FJsonValueNumber>(static_cast<double>(NumProp->GetSignedIntPropertyValue(ValuePtr)));
	}
	if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		return MakeShared<FJsonValueString>(StrProp->GetPropertyValue(ValuePtr));
	}
	if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		return MakeShared<FJsonValueString>(NameProp->GetPropertyValue(ValuePtr).ToString());
	}
	if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
	{
		return MakeShared<FJsonValueString>(TextProp->GetPropertyValue(ValuePtr).ToString());
	}
	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		const FName StructName = StructProp->Struct->GetFName();
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

		if (StructName == FName("Vector") || StructProp->Struct == TBaseStructure<FVector>::Get())
		{
			const FVector& V = *reinterpret_cast<const FVector*>(ValuePtr);
			Obj->SetNumberField(TEXT("x"), V.X);
			Obj->SetNumberField(TEXT("y"), V.Y);
			Obj->SetNumberField(TEXT("z"), V.Z);
		}
		else if (StructName == FName("Rotator") || StructProp->Struct == TBaseStructure<FRotator>::Get())
		{
			const FRotator& R = *reinterpret_cast<const FRotator*>(ValuePtr);
			Obj->SetNumberField(TEXT("pitch"), R.Pitch);
			Obj->SetNumberField(TEXT("yaw"),   R.Yaw);
			Obj->SetNumberField(TEXT("roll"),  R.Roll);
		}
		else if (StructName == FName("LinearColor") || StructProp->Struct == TBaseStructure<FLinearColor>::Get())
		{
			const FLinearColor& C = *reinterpret_cast<const FLinearColor*>(ValuePtr);
			Obj->SetNumberField(TEXT("r"), C.R);
			Obj->SetNumberField(TEXT("g"), C.G);
			Obj->SetNumberField(TEXT("b"), C.B);
			Obj->SetNumberField(TEXT("a"), C.A);
		}
		else if (StructName == FName("Color") || StructProp->Struct == TBaseStructure<FColor>::Get())
		{
			const FColor& C = *reinterpret_cast<const FColor*>(ValuePtr);
			Obj->SetNumberField(TEXT("r"), C.R);
			Obj->SetNumberField(TEXT("g"), C.G);
			Obj->SetNumberField(TEXT("b"), C.B);
			Obj->SetNumberField(TEXT("a"), C.A);
		}
		else
		{
			// Generic: export to string via UE ImportText format
			FString ExportedText;
			StructProp->ExportText_Direct(ExportedText, ValuePtr, nullptr, nullptr, PPF_None);
			return MakeShared<FJsonValueString>(ExportedText);
		}
		return MakeShared<FJsonValueObject>(Obj);
	}
	if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		UObject* Obj = ObjProp->GetObjectPropertyValue(ValuePtr);
		if (!Obj) return MakeShared<FJsonValueNull>();
		return MakeShared<FJsonValueString>(Obj->GetPathName());
	}

	// Fallback: export as string
	FString ExportedText;
	Property->ExportText_Direct(ExportedText, ValuePtr, nullptr, nullptr, PPF_None);
	return MakeShared<FJsonValueString>(ExportedText);
}
