// Copyright Natali Caggiano. All Rights Reserved.

#include "BlueprintGraphEditor.h"
#include "UnrealClaudeModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_ExecutionSequence.h"
#include "EdGraphSchema_K2.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "HAL/PlatformAtomics.h"
#include "K2Node_EnhancedInputAction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_SwitchInteger.h"
#include "K2Node_SwitchString.h"
#include "K2Node_SwitchEnum.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_MakeArray.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Select.h"
#include "K2Node_Timeline.h"
#include "Engine/TimelineTemplate.h"
#include "InputAction.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputSubsystems.h"

volatile int32 FBlueprintGraphEditor::NodeIdCounter = 0;
const FString FBlueprintGraphEditor::NodeIdPrefix = TEXT("MCP_ID:");

// ===== Graph Finding =====

UEdGraph* FBlueprintGraphEditor::FindGraph(
	UBlueprint* Blueprint,
	const FString& GraphName,
	bool bFunctionGraph,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return nullptr;
	}

	// UE 5.7 uses TObjectPtr for these arrays — auto& preserves the pointer wrapper
	auto& Graphs = bFunctionGraph ? Blueprint->FunctionGraphs : Blueprint->UbergraphPages;

	// Empty name means "use the default graph" (first in the array)
	if (GraphName.IsEmpty())
	{
		if (Graphs.Num() > 0 && Graphs[0])
		{
			return Graphs[0];
		}
		OutError = bFunctionGraph ? TEXT("No function graphs found") : TEXT("No event graphs found");
		return nullptr;
	}

	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName() == GraphName)
		{
			return Graph;
		}
	}

	TArray<FString> AvailableGraphs;
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph)
		{
			AvailableGraphs.Add(Graph->GetName());
		}
	}

	OutError = FString::Printf(TEXT("Graph '%s' not found. Available: %s"),
		*GraphName,
		*FString::Join(AvailableGraphs, TEXT(", ")));
	return nullptr;
}

// ===== Node Management =====

UEdGraphNode* FBlueprintGraphEditor::CreateNode(
	UEdGraph* Graph,
	const FString& NodeType,
	const TSharedPtr<FJsonObject>& NodeParams,
	int32 PosX,
	int32 PosY,
	FString& OutNodeId,
	FString& OutError)
{
	if (!Graph)
	{
		OutError = TEXT("Graph is null");
		return nullptr;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	if (!Blueprint)
	{
		OutError = TEXT("Could not find Blueprint for graph");
		return nullptr;
	}

	UEdGraphNode* NewNode = nullptr;
	FString Context;

	if (NodeType.Equals(TEXT("CallFunction"), ESearchCase::IgnoreCase))
	{
		FString FunctionName = NodeParams.IsValid() ? NodeParams->GetStringField(TEXT("function")) : TEXT("");
		FString TargetClass = NodeParams.IsValid() ? NodeParams->GetStringField(TEXT("target_class")) : TEXT("");
		Context = FunctionName;
		NewNode = CreateCallFunctionNode(Graph, FunctionName, TargetClass, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("Branch"), ESearchCase::IgnoreCase) || NodeType.Equals(TEXT("IfThenElse"), ESearchCase::IgnoreCase))
	{
		NewNode = CreateBranchNode(Graph, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("Event"), ESearchCase::IgnoreCase))
	{
		FString EventName = NodeParams.IsValid() ? NodeParams->GetStringField(TEXT("event")) : TEXT("");
		Context = EventName;
		NewNode = CreateEventNode(Graph, EventName, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("VariableGet"), ESearchCase::IgnoreCase) || NodeType.Equals(TEXT("GetVariable"), ESearchCase::IgnoreCase))
	{
		// Accept "variable" (canonical) or "variable_name" (common alias) — both are valid
		FString VariableName;
		if (NodeParams.IsValid())
		{
			VariableName = NodeParams->GetStringField(TEXT("variable"));
			if (VariableName.IsEmpty())
				VariableName = NodeParams->GetStringField(TEXT("variable_name"));
		}
		Context = VariableName;
		NewNode = CreateVariableGetNode(Graph, Blueprint, VariableName, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("VariableSet"), ESearchCase::IgnoreCase) || NodeType.Equals(TEXT("SetVariable"), ESearchCase::IgnoreCase))
	{
		// Accept "variable" (canonical) or "variable_name" (common alias) — both are valid
		FString VariableName;
		if (NodeParams.IsValid())
		{
			VariableName = NodeParams->GetStringField(TEXT("variable"));
			if (VariableName.IsEmpty())
				VariableName = NodeParams->GetStringField(TEXT("variable_name"));
		}
		Context = VariableName;
		NewNode = CreateVariableSetNode(Graph, Blueprint, VariableName, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("Sequence"), ESearchCase::IgnoreCase))
	{
		int32 NumOutputs = NodeParams.IsValid() ? (int32)NodeParams->GetNumberField(TEXT("num_outputs")) : 2;
		if (NumOutputs < 2) NumOutputs = 2;
		NewNode = CreateSequenceNode(Graph, NumOutputs, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("Add"), ESearchCase::IgnoreCase) ||
			 NodeType.Equals(TEXT("Subtract"), ESearchCase::IgnoreCase) ||
			 NodeType.Equals(TEXT("Multiply"), ESearchCase::IgnoreCase) ||
			 NodeType.Equals(TEXT("Divide"), ESearchCase::IgnoreCase))
	{
		Context = NodeType;
		NewNode = CreateMathNode(Graph, NodeType, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("PrintString"), ESearchCase::IgnoreCase))
	{
		// Convenience alias for CallFunction with PrintString
		Context = TEXT("PrintString");
		NewNode = CreateCallFunctionNode(Graph, TEXT("PrintString"), TEXT("KismetSystemLibrary"), PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("EnhancedInputAction"), ESearchCase::IgnoreCase))
	{
		FString ActionPath = NodeParams.IsValid() ? NodeParams->GetStringField(TEXT("action_path")) : TEXT("");
		Context = ActionPath;
		NewNode = CreateEnhancedInputActionNode(Graph, ActionPath, PosX, PosY, OutError);
	}
	// ---- Sprint 2 node types ----
	else if (NodeType.Equals(TEXT("Cast"), ESearchCase::IgnoreCase) ||
	         NodeType.Equals(TEXT("DynamicCast"), ESearchCase::IgnoreCase))
	{
		FString TargetClass = NodeParams.IsValid() ? NodeParams->GetStringField(TEXT("class")) : TEXT("");
		if (TargetClass.IsEmpty() && NodeParams.IsValid())
			TargetClass = NodeParams->GetStringField(TEXT("target_class"));
		Context = TargetClass;
		NewNode = CreateCastNode(Graph, TargetClass, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("ForEach"), ESearchCase::IgnoreCase) ||
	         NodeType.Equals(TEXT("ForEachLoop"), ESearchCase::IgnoreCase))
	{
		Context = TEXT("ForEachLoop");
		NewNode = CreateMacroNode(Graph, TEXT("ForEachLoop"), PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("ForEachWithBreak"), ESearchCase::IgnoreCase) ||
	         NodeType.Equals(TEXT("ForEachLoopWithBreak"), ESearchCase::IgnoreCase))
	{
		Context = TEXT("ForEachLoopWithBreak");
		NewNode = CreateMacroNode(Graph, TEXT("ForEachLoopWithBreak"), PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("DoOnce"), ESearchCase::IgnoreCase))
	{
		Context = TEXT("DoOnce");
		NewNode = CreateMacroNode(Graph, TEXT("DoOnce"), PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("Gate"), ESearchCase::IgnoreCase))
	{
		Context = TEXT("Gate");
		NewNode = CreateMacroNode(Graph, TEXT("Gate"), PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("Delay"), ESearchCase::IgnoreCase))
	{
		// Delay is a latent function on KismetSystemLibrary
		Context = TEXT("Delay");
		NewNode = CreateCallFunctionNode(Graph, TEXT("Delay"), TEXT("KismetSystemLibrary"), PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("Switch"), ESearchCase::IgnoreCase) ||
	         NodeType.Equals(TEXT("SwitchInt"), ESearchCase::IgnoreCase) ||
	         NodeType.Equals(TEXT("SwitchInteger"), ESearchCase::IgnoreCase) ||
	         NodeType.Equals(TEXT("SwitchString"), ESearchCase::IgnoreCase) ||
	         NodeType.Equals(TEXT("SwitchEnum"), ESearchCase::IgnoreCase))
	{
		FString SwitchOn = NodeType; // default derives type from node_type name
		FString EnumClass;
		if (NodeParams.IsValid())
		{
			FString ExplicitOn = NodeParams->GetStringField(TEXT("on")); // "int", "string", "enum"
			if (!ExplicitOn.IsEmpty()) SwitchOn = ExplicitOn;
			EnumClass = NodeParams->GetStringField(TEXT("enum_class"));
		}
		Context = SwitchOn;
		NewNode = CreateSwitchNode(Graph, SwitchOn, EnumClass, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("MakeStruct"), ESearchCase::IgnoreCase))
	{
		FString StructType = NodeParams.IsValid() ? NodeParams->GetStringField(TEXT("struct")) : TEXT("");
		if (StructType.IsEmpty() && NodeParams.IsValid())
			StructType = NodeParams->GetStringField(TEXT("struct_type"));
		Context = StructType;
		NewNode = CreateMakeStructNode(Graph, StructType, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("BreakStruct"), ESearchCase::IgnoreCase))
	{
		FString StructType = NodeParams.IsValid() ? NodeParams->GetStringField(TEXT("struct")) : TEXT("");
		if (StructType.IsEmpty() && NodeParams.IsValid())
			StructType = NodeParams->GetStringField(TEXT("struct_type"));
		Context = StructType;
		NewNode = CreateBreakStructNode(Graph, StructType, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("MakeArray"), ESearchCase::IgnoreCase))
	{
		FString ElementType = NodeParams.IsValid() ? NodeParams->GetStringField(TEXT("element_type")) : TEXT("float");
		Context = ElementType;
		NewNode = CreateMakeArrayNode(Graph, ElementType, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("CustomEvent"), ESearchCase::IgnoreCase))
	{
		FString EventName = NodeParams.IsValid() ? NodeParams->GetStringField(TEXT("event_name")) : TEXT("MyEvent");
		if (EventName.IsEmpty() && NodeParams.IsValid())
			EventName = NodeParams->GetStringField(TEXT("name"));
		if (EventName.IsEmpty()) EventName = TEXT("MyEvent");
		Context = EventName;
		NewNode = CreateCustomEventNode(Graph, EventName, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("Select"), ESearchCase::IgnoreCase))
	{
		NewNode = CreateSelectNode(Graph, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("Timeline"), ESearchCase::IgnoreCase))
	{
		FString TimelineName = NodeParams.IsValid() ? NodeParams->GetStringField(TEXT("timeline_name")) : TEXT("MyTimeline");
		if (TimelineName.IsEmpty() && NodeParams.IsValid())
			TimelineName = NodeParams->GetStringField(TEXT("name"));
		if (TimelineName.IsEmpty()) TimelineName = TEXT("MyTimeline");
		Context = TimelineName;
		NewNode = CreateTimelineNode(Graph, Blueprint, TimelineName, PosX, PosY, OutError);
	}
	else
	{
		OutError = FString::Printf(
			TEXT("Unknown node type: '%s'. Supported: CallFunction, Branch, Event, VariableGet, VariableSet, Sequence, "
			     "Add, Subtract, Multiply, Divide, PrintString, EnhancedInputAction, "
			     "Cast, ForEach, ForEachWithBreak, DoOnce, Gate, Delay, "
			     "Switch/SwitchInt/SwitchString/SwitchEnum, MakeStruct, BreakStruct, MakeArray, "
			     "CustomEvent, Select, Timeline"),
			*NodeType);
		return nullptr;
	}

	if (NewNode)
	{
		OutNodeId = GenerateNodeId(NodeType, Context, Graph);
		SetNodeId(NewNode, OutNodeId);

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

		UE_LOG(LogUnrealClaude, Log, TEXT("Created node '%s' (type: %s) at (%d, %d)"), *OutNodeId, *NodeType, PosX, PosY);
	}

	return NewNode;
}

bool FBlueprintGraphEditor::DeleteNode(UEdGraph* Graph, const FString& NodeId, FString& OutError)
{
	if (!Graph)
	{
		OutError = TEXT("Graph is null");
		return false;
	}

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node)
	{
		OutError = FString::Printf(TEXT("Node '%s' not found"), *NodeId);
		return false;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);

	// Break links before removal so dependent nodes don't end up with dangling pin references
	Node->BreakAllNodeLinks();
	Graph->RemoveNode(Node);

	if (Blueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Deleted node '%s'"), *NodeId);
	return true;
}

UEdGraphNode* FBlueprintGraphEditor::FindNodeById(UEdGraph* Graph, const FString& NodeId)
{
	if (!Graph || NodeId.IsEmpty())
	{
		return nullptr;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && GetNodeId(Node) == NodeId)
		{
			return Node;
		}
	}

	// Fallback: match against the node's native NodeGuid so pre-existing nodes
	// (created by the user or Engine, not by MCP) are also reachable by ID.
	// blueprint_query returns node_guid values for every node — this lets modify
	// ops (delete_node, connect_nodes, set_pin_default) accept the same IDs.
	FGuid ParsedGuid;
	if (FGuid::Parse(NodeId, ParsedGuid))
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->NodeGuid == ParsedGuid)
			{
				return Node;
			}
		}
	}

	return nullptr;
}

// ===== Pin & Connection Management =====

bool FBlueprintGraphEditor::ConnectPins(
	UEdGraph* Graph,
	const FString& SourceNodeId,
	const FString& SourcePinName,
	const FString& TargetNodeId,
	const FString& TargetPinName,
	FString& OutError)
{
	if (!Graph)
	{
		OutError = TEXT("Graph is null");
		return false;
	}

	UEdGraphNode* SourceNode = FindNodeById(Graph, SourceNodeId);
	if (!SourceNode)
	{
		OutError = FString::Printf(TEXT("Source node '%s' not found"), *SourceNodeId);
		return false;
	}

	UEdGraphNode* TargetNode = FindNodeById(Graph, TargetNodeId);
	if (!TargetNode)
	{
		OutError = FString::Printf(TEXT("Target node '%s' not found"), *TargetNodeId);
		return false;
	}

	// Empty pin names trigger exec-pin auto-detection so callers can wire exec flow without naming pins
	UEdGraphPin* SourcePin = nullptr;
	UEdGraphPin* TargetPin = nullptr;

	if (SourcePinName.IsEmpty())
	{
		SourcePin = GetExecPin(SourceNode, true);
		if (!SourcePin)
		{
			OutError = FString::Printf(TEXT("No exec output pin found on node '%s'"), *SourceNodeId);
			return false;
		}
	}
	else
	{
		SourcePin = FindPinByName(SourceNode, SourcePinName, EGPD_Output);
		if (!SourcePin)
		{
			// Fall back to input direction for bidirectional data pins
			SourcePin = FindPinByName(SourceNode, SourcePinName, EGPD_Input);
		}
		if (!SourcePin)
		{
			OutError = FString::Printf(TEXT("Pin '%s' not found on source node '%s'"), *SourcePinName, *SourceNodeId);
			return false;
		}
	}

	if (TargetPinName.IsEmpty())
	{
		TargetPin = GetExecPin(TargetNode, false);
		if (!TargetPin)
		{
			OutError = FString::Printf(TEXT("No exec input pin found on node '%s'"), *TargetNodeId);
			return false;
		}
	}
	else
	{
		TargetPin = FindPinByName(TargetNode, TargetPinName, EGPD_Input);
		if (!TargetPin)
		{
			// Fall back to output direction for bidirectional data pins
			TargetPin = FindPinByName(TargetNode, TargetPinName, EGPD_Output);
		}
		if (!TargetPin)
		{
			OutError = FString::Printf(TEXT("Pin '%s' not found on target node '%s'"), *TargetPinName, *TargetNodeId);
			return false;
		}
	}

	const UEdGraphSchema* Schema = Graph->GetSchema();
	if (Schema)
	{
		FPinConnectionResponse Response = Schema->CanCreateConnection(SourcePin, TargetPin);
		if (Response.Response == CONNECT_RESPONSE_DISALLOW)
		{
			OutError = FString::Printf(TEXT("Cannot connect pins: %s"), *Response.Message.ToString());
			return false;
		}
	}

	SourcePin->MakeLinkTo(TargetPin);

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	if (Blueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Connected '%s.%s' -> '%s.%s'"),
		*SourceNodeId, *SourcePin->PinName.ToString(),
		*TargetNodeId, *TargetPin->PinName.ToString());

	return true;
}

bool FBlueprintGraphEditor::DisconnectPins(
	UEdGraph* Graph,
	const FString& SourceNodeId,
	const FString& SourcePinName,
	const FString& TargetNodeId,
	const FString& TargetPinName,
	FString& OutError)
{
	if (!Graph)
	{
		OutError = TEXT("Graph is null");
		return false;
	}

	UEdGraphNode* SourceNode = FindNodeById(Graph, SourceNodeId);
	if (!SourceNode)
	{
		OutError = FString::Printf(TEXT("Source node '%s' not found"), *SourceNodeId);
		return false;
	}

	UEdGraphNode* TargetNode = FindNodeById(Graph, TargetNodeId);
	if (!TargetNode)
	{
		OutError = FString::Printf(TEXT("Target node '%s' not found"), *TargetNodeId);
		return false;
	}

	UEdGraphPin* SourcePin = FindPinByName(SourceNode, SourcePinName, EGPD_MAX);
	if (!SourcePin)
	{
		OutError = FString::Printf(TEXT("Pin '%s' not found on source node '%s'"), *SourcePinName, *SourceNodeId);
		return false;
	}

	UEdGraphPin* TargetPin = FindPinByName(TargetNode, TargetPinName, EGPD_MAX);
	if (!TargetPin)
	{
		OutError = FString::Printf(TEXT("Pin '%s' not found on target node '%s'"), *TargetPinName, *TargetNodeId);
		return false;
	}

	SourcePin->BreakLinkTo(TargetPin);

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	if (Blueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Disconnected '%s.%s' from '%s.%s'"),
		*SourceNodeId, *SourcePinName,
		*TargetNodeId, *TargetPinName);

	return true;
}

bool FBlueprintGraphEditor::SetPinDefaultValue(
	UEdGraph* Graph,
	const FString& NodeId,
	const FString& PinName,
	const FString& Value,
	FString& OutError)
{
	if (!Graph)
	{
		OutError = TEXT("Graph is null");
		return false;
	}

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node)
	{
		OutError = FString::Printf(TEXT("Node '%s' not found"), *NodeId);
		return false;
	}

	UEdGraphPin* Pin = FindPinByName(Node, PinName, EGPD_Input);
	if (!Pin)
	{
		OutError = FString::Printf(TEXT("Input pin '%s' not found on node '%s'"), *PinName, *NodeId);
		return false;
	}

	const UEdGraphSchema* Schema = Graph->GetSchema();
	if (Schema)
	{
		Schema->TrySetDefaultValue(*Pin, Value);
	}
	else
	{
		Pin->DefaultValue = Value;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	if (Blueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Set pin '%s.%s' default value to '%s'"), *NodeId, *PinName, *Value);
	return true;
}

UEdGraphPin* FBlueprintGraphEditor::FindPinByName(
	UEdGraphNode* Node,
	const FString& PinName,
	EEdGraphPinDirection Direction)
{
	if (!Node)
	{
		return nullptr;
	}

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
		{
			if (Direction == EGPD_MAX || Pin->Direction == Direction)
			{
				return Pin;
			}
		}
	}

	return nullptr;
}

UEdGraphPin* FBlueprintGraphEditor::GetExecPin(UEdGraphNode* Node, bool bOutput)
{
	if (!Node)
	{
		return nullptr;
	}

	EEdGraphPinDirection Direction = bOutput ? EGPD_Output : EGPD_Input;

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == Direction && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			return Pin;
		}
	}

	return nullptr;
}

// ===== Serialization =====

TSharedPtr<FJsonObject> FBlueprintGraphEditor::SerializeNodeInfo(UEdGraphNode* Node)
{
	TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();

	if (!Node)
	{
		return NodeObj;
	}

	NodeObj->SetStringField(TEXT("node_id"), GetNodeId(Node));
	NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
	NodeObj->SetNumberField(TEXT("pos_x"), Node->NodePosX);
	NodeObj->SetNumberField(TEXT("pos_y"), Node->NodePosY);

	TArray<TSharedPtr<FJsonValue>> Pins;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin)
		{
			TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
			PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
			PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));

			FString TypeStr;
			if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
			{
				TypeStr = TEXT("bool");
			}
			else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
			{
				TypeStr = TEXT("int32");
			}
			else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Real)
			{
				TypeStr = (Pin->PinType.PinSubCategory == UEdGraphSchema_K2::PC_Double) ? TEXT("double") : TEXT("float");
			}
			else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_String)
			{
				TypeStr = TEXT("FString");
			}
			else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				TypeStr = TEXT("exec");
			}
			else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
			{
				if (UScriptStruct* Struct = Cast<UScriptStruct>(Pin->PinType.PinSubCategoryObject.Get()))
				{
					TypeStr = Struct->GetName();
				}
			}
			else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
			{
				if (UClass* Class = Cast<UClass>(Pin->PinType.PinSubCategoryObject.Get()))
				{
					TypeStr = Class->GetName() + TEXT("*");
				}
			}
			else
			{
				TypeStr = Pin->PinType.PinCategory.ToString();
			}

			PinObj->SetStringField(TEXT("type"), TypeStr);
			if (!Pin->DefaultValue.IsEmpty())
			{
				PinObj->SetStringField(TEXT("default_value"), Pin->DefaultValue);
			}
			PinObj->SetNumberField(TEXT("connections"), Pin->LinkedTo.Num());
			Pins.Add(MakeShared<FJsonValueObject>(PinObj));
		}
	}
	NodeObj->SetArrayField(TEXT("pins"), Pins);

	return NodeObj;
}

// ===== Node ID System =====

FString FBlueprintGraphEditor::GenerateNodeId(const FString& NodeType, const FString& Context, UEdGraph* Graph)
{
	FString BaseId;
	if (Context.IsEmpty())
	{
		BaseId = NodeType;
	}
	else
	{
		BaseId = FString::Printf(TEXT("%s_%s"), *NodeType, *Context);
	}

	// Atomic increment makes IDs unique across concurrent MCP tool calls
	int32 Counter = FPlatformAtomics::InterlockedIncrement(&NodeIdCounter);
	FString NodeId = FString::Printf(TEXT("%s_%d"), *BaseId, Counter);

	if (Graph)
	{
		while (FindNodeById(Graph, NodeId) != nullptr)
		{
			Counter = FPlatformAtomics::InterlockedIncrement(&NodeIdCounter);
			NodeId = FString::Printf(TEXT("%s_%d"), *BaseId, Counter);
		}
	}

	return NodeId;
}

void FBlueprintGraphEditor::SetNodeId(UEdGraphNode* Node, const FString& NodeId)
{
	if (Node)
	{
		// Store ID in node comment because it persists with the asset and is visible in the editor
		Node->NodeComment = NodeIdPrefix + NodeId;
	}
}

FString FBlueprintGraphEditor::GetNodeId(UEdGraphNode* Node)
{
	if (!Node)
	{
		return FString();
	}

	if (Node->NodeComment.StartsWith(NodeIdPrefix))
	{
		return Node->NodeComment.RightChop(NodeIdPrefix.Len());
	}

	return FString();
}

// ===== Private Node Creation Helpers =====

UClass* FBlueprintGraphEditor::FindClassByShortName(const FString& ClassName)
{
	if (ClassName.IsEmpty()) return nullptr;

	// Already a full path (contains '.')
	if (ClassName.Contains(TEXT(".")))
	{
		UClass* C = FindObject<UClass>(nullptr, *ClassName);
		if (C) return C;
	}

	// Hardcoded common engine classes by short name
	static TMap<FString, UClass*> CommonClasses;
	static bool bInitialised = false;
	if (!bInitialised)
	{
		bInitialised = true;
		CommonClasses.Add(TEXT("Actor"),            AActor::StaticClass());
		CommonClasses.Add(TEXT("AActor"),           AActor::StaticClass());
		CommonClasses.Add(TEXT("Pawn"),             APawn::StaticClass());
		CommonClasses.Add(TEXT("APawn"),            APawn::StaticClass());
		CommonClasses.Add(TEXT("Controller"),       AController::StaticClass());
		CommonClasses.Add(TEXT("AController"),      AController::StaticClass());
		CommonClasses.Add(TEXT("PlayerController"), APlayerController::StaticClass());
		CommonClasses.Add(TEXT("APlayerController"),APlayerController::StaticClass());
		CommonClasses.Add(TEXT("ActorComponent"),   UActorComponent::StaticClass());
		CommonClasses.Add(TEXT("SceneComponent"),   USceneComponent::StaticClass());
		CommonClasses.Add(TEXT("KismetSystemLibrary"), UKismetSystemLibrary::StaticClass());
		CommonClasses.Add(TEXT("KismetMathLibrary"),   UKismetMathLibrary::StaticClass());
		CommonClasses.Add(TEXT("GameplayStatics"),     UGameplayStatics::StaticClass());
		CommonClasses.Add(TEXT("UGameplayStatics"),    UGameplayStatics::StaticClass());
		CommonClasses.Add(TEXT("EnhancedInputLocalPlayerSubsystem"), UEnhancedInputLocalPlayerSubsystem::StaticClass());
	}
	if (UClass** Found = CommonClasses.Find(ClassName))
	{
		return *Found;
	}

	// Try script package paths
	static const TArray<FString> PackagePrefixes = {
		TEXT("/Script/Engine."),
		TEXT("/Script/CoreUObject."),
		TEXT("/Script/EnhancedInput."),
		TEXT("/Script/UMG."),
		TEXT("/Script/AIModule."),
		TEXT("/Script/GameplayAbilities."),
	};
	for (const FString& Prefix : PackagePrefixes)
	{
		UClass* C = FindObject<UClass>(nullptr, *(Prefix + ClassName));
		if (C) return C;
	}

	return nullptr;
}

UEdGraphNode* FBlueprintGraphEditor::CreateCallFunctionNode(
	UEdGraph* Graph,
	const FString& FunctionName,
	const FString& TargetClass,
	int32 PosX,
	int32 PosY,
	FString& OutError)
{
	if (FunctionName.IsEmpty())
	{
		OutError = TEXT("Function name is required");
		return nullptr;
	}

	UFunction* Function = nullptr;

	// 1. If target_class specified, try it first
	if (!TargetClass.IsEmpty())
	{
		UClass* Owner = FindClassByShortName(TargetClass);
		if (Owner)
		{
			// Search including inherited functions
			Function = Owner->FindFunctionByName(FName(*FunctionName), EIncludeSuperFlag::IncludeSuper);
		}
	}

	// 2. Blueprint's own generated class (self-functions like custom BP functions)
	UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	if (!Function && BP && BP->GeneratedClass)
	{
		Function = BP->GeneratedClass->FindFunctionByName(FName(*FunctionName), EIncludeSuperFlag::IncludeSuper);
	}

	// 3. Kismet libraries (IncludeSuper so inherited/interface methods are found)
	if (!Function)
	{
		Function = UKismetSystemLibrary::StaticClass()->FindFunctionByName(FName(*FunctionName), EIncludeSuperFlag::IncludeSuper);
	}
	if (!Function)
	{
		Function = UKismetMathLibrary::StaticClass()->FindFunctionByName(FName(*FunctionName), EIncludeSuperFlag::IncludeSuper);
	}

	// 3b. GameplayStatics and EnhancedInput subsystem
	if (!Function)
	{
		Function = UGameplayStatics::StaticClass()->FindFunctionByName(FName(*FunctionName), EIncludeSuperFlag::IncludeSuper);
	}
	if (!Function)
	{
		Function = UEnhancedInputLocalPlayerSubsystem::StaticClass()->FindFunctionByName(FName(*FunctionName), EIncludeSuperFlag::IncludeSuper);
	}

	// 4. Common engine actor/component classes
	if (!Function)
	{
		static const TArray<UClass*> EngineSearchClasses = {
			AActor::StaticClass(),
			APawn::StaticClass(),
			AController::StaticClass(),
			APlayerController::StaticClass(),
			UActorComponent::StaticClass(),
			USceneComponent::StaticClass(),
		};
		for (UClass* SearchClass : EngineSearchClasses)
		{
			Function = SearchClass->FindFunctionByName(FName(*FunctionName), EIncludeSuperFlag::IncludeSuper);
			if (Function) break;
		}
	}

	if (!Function)
	{
		// Build fuzzy suggestions: substring-match FunctionName across all known classes
		static const TArray<UClass*> AllSearchClasses = {
			UKismetSystemLibrary::StaticClass(),
			UKismetMathLibrary::StaticClass(),
			UGameplayStatics::StaticClass(),
			UEnhancedInputLocalPlayerSubsystem::StaticClass(),
			AActor::StaticClass(),
			APawn::StaticClass(),
			AController::StaticClass(),
			APlayerController::StaticClass(),
			UActorComponent::StaticClass(),
			USceneComponent::StaticClass(),
		};

		TArray<FString> Suggestions;
		const FString LowerQuery = FunctionName.ToLower();
		for (UClass* SearchClass : AllSearchClasses)
		{
			if (!SearchClass) continue;
			for (TFieldIterator<UFunction> It(SearchClass, EFieldIteratorFlags::IncludeSuper); It; ++It)
			{
				UFunction* Candidate = *It;
				if (!Candidate || !(Candidate->FunctionFlags & FUNC_BlueprintCallable)) continue;
				const FString CandidateName = Candidate->GetName();
				if (CandidateName.ToLower().Contains(LowerQuery))
				{
					Suggestions.Add(FString::Printf(TEXT("%s (class: %s)"), *CandidateName, *SearchClass->GetName()));
					if (Suggestions.Num() >= 5) break;
				}
			}
			if (Suggestions.Num() >= 5) break;
		}

		if (Suggestions.Num() > 0)
		{
			OutError = FString::Printf(
				TEXT("Function '%s' not found. Did you mean: %s"),
				*FunctionName,
				*FString::Join(Suggestions, TEXT(" | ")));
		}
		else
		{
			OutError = FString::Printf(
				TEXT("Function '%s' not found. Tip: use blueprint_query 'find_function' op to search all registered UFUNCTIONs."),
				*FunctionName);
		}
		return nullptr;
	}

	FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(*Graph);
	UK2Node_CallFunction* CallNode = NodeCreator.CreateNode();
	CallNode->SetFromFunction(Function);
	CallNode->NodePosX = PosX;
	CallNode->NodePosY = PosY;
	NodeCreator.Finalize();

	return CallNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateBranchNode(
	UEdGraph* Graph,
	int32 PosX,
	int32 PosY,
	FString& OutError)
{
	FGraphNodeCreator<UK2Node_IfThenElse> NodeCreator(*Graph);
	UK2Node_IfThenElse* BranchNode = NodeCreator.CreateNode();
	BranchNode->NodePosX = PosX;
	BranchNode->NodePosY = PosY;
	NodeCreator.Finalize();

	return BranchNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateEventNode(
	UEdGraph* Graph,
	const FString& EventName,
	int32 PosX,
	int32 PosY,
	FString& OutError)
{
	if (EventName.IsEmpty())
	{
		OutError = TEXT("Event name is required");
		return nullptr;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	if (!Blueprint)
	{
		OutError = TEXT("Could not find Blueprint for graph");
		return nullptr;
	}

	UFunction* EventFunc = nullptr;

	if (EventName.Equals(TEXT("BeginPlay"), ESearchCase::IgnoreCase))
	{
		EventFunc = AActor::StaticClass()->FindFunctionByName(FName("ReceiveBeginPlay"));
	}
	else if (EventName.Equals(TEXT("Tick"), ESearchCase::IgnoreCase))
	{
		EventFunc = AActor::StaticClass()->FindFunctionByName(FName("ReceiveTick"));
	}
	else if (EventName.Equals(TEXT("EndPlay"), ESearchCase::IgnoreCase))
	{
		EventFunc = AActor::StaticClass()->FindFunctionByName(FName("ReceiveEndPlay"));
	}
	else
	{
		if (Blueprint->ParentClass)
		{
			EventFunc = Blueprint->ParentClass->FindFunctionByName(FName(*EventName));
		}
	}

	if (!EventFunc)
	{
		OutError = FString::Printf(TEXT("Event '%s' not found. Common events: BeginPlay, Tick, EndPlay"), *EventName);
		return nullptr;
	}

	FGraphNodeCreator<UK2Node_Event> NodeCreator(*Graph);
	UK2Node_Event* EventNode = NodeCreator.CreateNode();
	EventNode->EventReference.SetFromField<UFunction>(EventFunc, false);
	EventNode->bOverrideFunction = true;
	EventNode->NodePosX = PosX;
	EventNode->NodePosY = PosY;
	NodeCreator.Finalize();

	return EventNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateVariableGetNode(
	UEdGraph* Graph,
	UBlueprint* Blueprint,
	const FString& VariableName,
	int32 PosX,
	int32 PosY,
	FString& OutError)
{
	if (VariableName.IsEmpty())
	{
		OutError = TEXT("Variable name is required");
		return nullptr;
	}

	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return nullptr;
	}

	FName VarName(*VariableName);
	bool bFound = false;
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarName == VarName)
		{
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		OutError = FString::Printf(TEXT("Variable '%s' not found in Blueprint"), *VariableName);
		return nullptr;
	}

	FGraphNodeCreator<UK2Node_VariableGet> NodeCreator(*Graph);
	UK2Node_VariableGet* GetNode = NodeCreator.CreateNode();
	GetNode->VariableReference.SetSelfMember(VarName);
	GetNode->NodePosX = PosX;
	GetNode->NodePosY = PosY;
	NodeCreator.Finalize();

	return GetNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateVariableSetNode(
	UEdGraph* Graph,
	UBlueprint* Blueprint,
	const FString& VariableName,
	int32 PosX,
	int32 PosY,
	FString& OutError)
{
	if (VariableName.IsEmpty())
	{
		OutError = TEXT("Variable name is required");
		return nullptr;
	}

	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return nullptr;
	}

	FName VarName(*VariableName);
	bool bFound = false;
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarName == VarName)
		{
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		OutError = FString::Printf(TEXT("Variable '%s' not found in Blueprint"), *VariableName);
		return nullptr;
	}

	FGraphNodeCreator<UK2Node_VariableSet> NodeCreator(*Graph);
	UK2Node_VariableSet* SetNode = NodeCreator.CreateNode();
	SetNode->VariableReference.SetSelfMember(VarName);
	SetNode->NodePosX = PosX;
	SetNode->NodePosY = PosY;
	NodeCreator.Finalize();

	return SetNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateSequenceNode(
	UEdGraph* Graph,
	int32 NumOutputs,
	int32 PosX,
	int32 PosY,
	FString& OutError)
{
	FGraphNodeCreator<UK2Node_ExecutionSequence> NodeCreator(*Graph);
	UK2Node_ExecutionSequence* SeqNode = NodeCreator.CreateNode();
	SeqNode->NodePosX = PosX;
	SeqNode->NodePosY = PosY;
	NodeCreator.Finalize();

	while (SeqNode->Pins.Num() < NumOutputs + 1) // +1 for input exec
	{
		SeqNode->AddInputPin();
	}

	return SeqNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateEnhancedInputActionNode(
	UEdGraph* Graph,
	const FString& ActionPath,
	int32 PosX,
	int32 PosY,
	FString& OutError)
{
	FGraphNodeCreator<UK2Node_EnhancedInputAction> NodeCreator(*Graph);
	UK2Node_EnhancedInputAction* InputNode = NodeCreator.CreateNode();

	if (!ActionPath.IsEmpty())
	{
		UInputAction* InputAction = LoadObject<UInputAction>(nullptr, *ActionPath);
		if (!InputAction)
		{
			// Try appending the asset name (e.g. /Game/Controls/IA_Key1 -> /Game/Controls/IA_Key1.IA_Key1)
			FString AssetName = FPaths::GetBaseFilename(ActionPath);
			FString FullPath = ActionPath + TEXT(".") + AssetName;
			InputAction = LoadObject<UInputAction>(nullptr, *FullPath);
		}
		if (InputAction)
		{
			InputNode->InputAction = InputAction;
		}
		else
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("CreateEnhancedInputActionNode: InputAction not found at '%s' — node created without action reference"), *ActionPath);
		}
	}

	InputNode->NodePosX = PosX;
	InputNode->NodePosY = PosY;
	NodeCreator.Finalize();

	return InputNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateMathNode(
	UEdGraph* Graph,
	const FString& MathOp,
	int32 PosX,
	int32 PosY,
	FString& OutError)
{
	// UE5 promoted float→double; try float variant first, fall back to double variant
	FName FunctionName;
	FName FunctionNameDouble;
	if (MathOp.Equals(TEXT("Add"), ESearchCase::IgnoreCase))
	{
		FunctionName       = FName("Add_FloatFloat");
		FunctionNameDouble = FName("Add_DoubleDouble");
	}
	else if (MathOp.Equals(TEXT("Subtract"), ESearchCase::IgnoreCase))
	{
		FunctionName       = FName("Subtract_FloatFloat");
		FunctionNameDouble = FName("Subtract_DoubleDouble");
	}
	else if (MathOp.Equals(TEXT("Multiply"), ESearchCase::IgnoreCase))
	{
		FunctionName       = FName("Multiply_FloatFloat");
		FunctionNameDouble = FName("Multiply_DoubleDouble");
	}
	else if (MathOp.Equals(TEXT("Divide"), ESearchCase::IgnoreCase))
	{
		FunctionName       = FName("Divide_FloatFloat");
		FunctionNameDouble = FName("Divide_DoubleDouble");
	}
	else
	{
		OutError = FString::Printf(TEXT("Unknown math operation: '%s'. Supported: Add, Subtract, Multiply, Divide"), *MathOp);
		return nullptr;
	}

	UFunction* MathFunc = UKismetMathLibrary::StaticClass()->FindFunctionByName(FunctionName, EIncludeSuperFlag::IncludeSuper);
	if (!MathFunc)
	{
		MathFunc = UKismetMathLibrary::StaticClass()->FindFunctionByName(FunctionNameDouble, EIncludeSuperFlag::IncludeSuper);
	}
	if (!MathFunc)
	{
		OutError = FString::Printf(TEXT("Math function '%s' (or '%s') not found"), *FunctionName.ToString(), *FunctionNameDouble.ToString());
		return nullptr;
	}

	FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(*Graph);
	UK2Node_CallFunction* MathNode = NodeCreator.CreateNode();
	MathNode->SetFromFunction(MathFunc);
	MathNode->NodePosX = PosX;
	MathNode->NodePosY = PosY;
	NodeCreator.Finalize();

	return MathNode;
}

// ===== Sprint 2: New Node Types =====

UScriptStruct* FBlueprintGraphEditor::FindStructByShortName(const FString& StructName)
{
	if (StructName == TEXT("FVector") || StructName == TEXT("Vector"))
		return TBaseStructure<FVector>::Get();
	if (StructName == TEXT("FRotator") || StructName == TEXT("Rotator"))
		return TBaseStructure<FRotator>::Get();
	if (StructName == TEXT("FTransform") || StructName == TEXT("Transform"))
		return TBaseStructure<FTransform>::Get();
	if (StructName == TEXT("FLinearColor") || StructName == TEXT("LinearColor"))
		return TBaseStructure<FLinearColor>::Get();
	if (StructName == TEXT("FColor") || StructName == TEXT("Color"))
		return TBaseStructure<FColor>::Get();
	if (StructName == TEXT("FVector2D") || StructName == TEXT("Vector2D"))
		return TBaseStructure<FVector2D>::Get();
	if (StructName == TEXT("FHitResult") || StructName == TEXT("HitResult"))
		return TBaseStructure<FHitResult>::Get();

	// Generic search — try common script packages
	for (const FString& Prefix : TArray<FString>{
		TEXT("/Script/CoreUObject."),
		TEXT("/Script/Engine."),
		TEXT("/Script/EnhancedInput.")})
	{
		UScriptStruct* Found = FindObject<UScriptStruct>(nullptr, *(Prefix + StructName));
		if (Found) return Found;
	}

	return FindObject<UScriptStruct>(nullptr, *StructName);
}

UEdGraphNode* FBlueprintGraphEditor::CreateCastNode(
	UEdGraph* Graph,
	const FString& TargetClass,
	int32 PosX, int32 PosY,
	FString& OutError)
{
	if (TargetClass.IsEmpty())
	{
		OutError = TEXT("Cast node requires 'class' or 'target_class' param (e.g. \"PlayerController\")");
		return nullptr;
	}

	UClass* Class = FindClassByShortName(TargetClass);
	if (!Class)
	{
		OutError = FString::Printf(TEXT("Cast: class '%s' not found"), *TargetClass);
		return nullptr;
	}

	FGraphNodeCreator<UK2Node_DynamicCast> NodeCreator(*Graph);
	UK2Node_DynamicCast* CastNode = NodeCreator.CreateNode();
	CastNode->TargetType = Class;
	CastNode->NodePosX = PosX;
	CastNode->NodePosY = PosY;
	NodeCreator.Finalize();

	return CastNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateMacroNode(
	UEdGraph* Graph,
	const FString& MacroName,
	int32 PosX, int32 PosY,
	FString& OutError)
{
	// Standard macros are in the engine's StandardMacros blueprint
	static const TCHAR* MacroLibPath = TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros");
	UBlueprint* MacroLib = Cast<UBlueprint>(
		StaticLoadObject(UBlueprint::StaticClass(), nullptr, MacroLibPath));

	if (!MacroLib)
	{
		OutError = FString::Printf(
			TEXT("Could not load StandardMacros library. Macro '%s' unavailable."), *MacroName);
		return nullptr;
	}

	UEdGraph* MacroGraph = nullptr;
	for (UEdGraph* G : MacroLib->MacroGraphs)
	{
		if (G && G->GetName().Equals(MacroName, ESearchCase::IgnoreCase))
		{
			MacroGraph = G;
			break;
		}
	}

	if (!MacroGraph)
	{
		// Build list of available macros for helpful error
		TArray<FString> Available;
		for (UEdGraph* G : MacroLib->MacroGraphs)
		{
			if (G) Available.Add(G->GetName());
		}
		OutError = FString::Printf(TEXT("Macro '%s' not found in StandardMacros. Available: %s"),
			*MacroName, *FString::Join(Available, TEXT(", ")));
		return nullptr;
	}

	FGraphNodeCreator<UK2Node_MacroInstance> NodeCreator(*Graph);
	UK2Node_MacroInstance* MacroNode = NodeCreator.CreateNode();
	MacroNode->SetMacroGraph(MacroGraph);
	MacroNode->NodePosX = PosX;
	MacroNode->NodePosY = PosY;
	NodeCreator.Finalize();

	return MacroNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateSwitchNode(
	UEdGraph* Graph,
	const FString& SwitchOn,
	const FString& EnumClass,
	int32 PosX, int32 PosY,
	FString& OutError)
{
	const FString Lower = SwitchOn.ToLower();

	if (Lower.Contains(TEXT("string")))
	{
		FGraphNodeCreator<UK2Node_SwitchString> NodeCreator(*Graph);
		UK2Node_SwitchString* SwitchNode = NodeCreator.CreateNode();
		SwitchNode->NodePosX = PosX;
		SwitchNode->NodePosY = PosY;
		NodeCreator.Finalize();
		return SwitchNode;
	}
	else if (Lower.Contains(TEXT("enum")))
	{
		if (EnumClass.IsEmpty())
		{
			OutError = TEXT("SwitchEnum requires 'enum_class' param (e.g. \"EMyEnum\")");
			return nullptr;
		}
		UEnum* Enum = FindObject<UEnum>(nullptr, *EnumClass);
		if (!Enum)
		{
			// Try script paths
			for (const FString& Prefix : TArray<FString>{TEXT("/Script/Engine."), TEXT("/Script/CoreUObject.")})
			{
				Enum = FindObject<UEnum>(nullptr, *(Prefix + EnumClass));
				if (Enum) break;
			}
		}
		if (!Enum)
		{
			OutError = FString::Printf(TEXT("SwitchEnum: enum '%s' not found"), *EnumClass);
			return nullptr;
		}

		FGraphNodeCreator<UK2Node_SwitchEnum> NodeCreator(*Graph);
		UK2Node_SwitchEnum* SwitchNode = NodeCreator.CreateNode();
		SwitchNode->Enum = Enum;
		SwitchNode->NodePosX = PosX;
		SwitchNode->NodePosY = PosY;
		NodeCreator.Finalize();
		return SwitchNode;
	}
	else
	{
		// Default: switch on int
		FGraphNodeCreator<UK2Node_SwitchInteger> NodeCreator(*Graph);
		UK2Node_SwitchInteger* SwitchNode = NodeCreator.CreateNode();
		SwitchNode->NodePosX = PosX;
		SwitchNode->NodePosY = PosY;
		NodeCreator.Finalize();
		return SwitchNode;
	}
}

UEdGraphNode* FBlueprintGraphEditor::CreateMakeStructNode(
	UEdGraph* Graph,
	const FString& StructType,
	int32 PosX, int32 PosY,
	FString& OutError)
{
	if (StructType.IsEmpty())
	{
		OutError = TEXT("MakeStruct requires 'struct' param (e.g. \"FVector\", \"FHitResult\")");
		return nullptr;
	}

	UScriptStruct* Struct = FindStructByShortName(StructType);
	if (!Struct)
	{
		OutError = FString::Printf(TEXT("MakeStruct: struct '%s' not found"), *StructType);
		return nullptr;
	}

	FGraphNodeCreator<UK2Node_MakeStruct> NodeCreator(*Graph);
	UK2Node_MakeStruct* MakeNode = NodeCreator.CreateNode();
	MakeNode->StructType = Struct;
	MakeNode->NodePosX = PosX;
	MakeNode->NodePosY = PosY;
	NodeCreator.Finalize();

	return MakeNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateBreakStructNode(
	UEdGraph* Graph,
	const FString& StructType,
	int32 PosX, int32 PosY,
	FString& OutError)
{
	if (StructType.IsEmpty())
	{
		OutError = TEXT("BreakStruct requires 'struct' param (e.g. \"FVector\", \"FHitResult\")");
		return nullptr;
	}

	UScriptStruct* Struct = FindStructByShortName(StructType);
	if (!Struct)
	{
		OutError = FString::Printf(TEXT("BreakStruct: struct '%s' not found"), *StructType);
		return nullptr;
	}

	FGraphNodeCreator<UK2Node_BreakStruct> NodeCreator(*Graph);
	UK2Node_BreakStruct* BreakNode = NodeCreator.CreateNode();
	BreakNode->StructType = Struct;
	BreakNode->NodePosX = PosX;
	BreakNode->NodePosY = PosY;
	NodeCreator.Finalize();

	return BreakNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateMakeArrayNode(
	UEdGraph* Graph,
	const FString& ElementType,
	int32 PosX, int32 PosY,
	FString& OutError)
{
	FGraphNodeCreator<UK2Node_MakeArray> NodeCreator(*Graph);
	UK2Node_MakeArray* MakeArrNode = NodeCreator.CreateNode();
	MakeArrNode->NodePosX = PosX;
	MakeArrNode->NodePosY = PosY;
	NodeCreator.Finalize();

	// Pin type is inferred from first connection; no forced type needed at creation
	return MakeArrNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateCustomEventNode(
	UEdGraph* Graph,
	const FString& EventName,
	int32 PosX, int32 PosY,
	FString& OutError)
{
	FGraphNodeCreator<UK2Node_CustomEvent> NodeCreator(*Graph);
	UK2Node_CustomEvent* EventNode = NodeCreator.CreateNode();
	EventNode->CustomFunctionName = FName(*EventName);
	EventNode->NodePosX = PosX;
	EventNode->NodePosY = PosY;
	NodeCreator.Finalize();

	return EventNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateSelectNode(
	UEdGraph* Graph,
	int32 PosX, int32 PosY,
	FString& OutError)
{
	FGraphNodeCreator<UK2Node_Select> NodeCreator(*Graph);
	UK2Node_Select* SelectNode = NodeCreator.CreateNode();
	SelectNode->NodePosX = PosX;
	SelectNode->NodePosY = PosY;
	NodeCreator.Finalize();

	return SelectNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateTimelineNode(
	UEdGraph* Graph,
	UBlueprint* Blueprint,
	const FString& TimelineName,
	int32 PosX, int32 PosY,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Timeline node requires a valid Blueprint");
		return nullptr;
	}

	// Ensure unique name: if one with this name already exists, append counter
	FName FinalName(*TimelineName);
	{
		int32 Suffix = 1;
		auto TimelineExists = [&](const FName& N) -> bool {
			for (UTimelineTemplate* T : Blueprint->Timelines)
				if (T && T->GetVariableName() == N) return true;
			return false;
		};
		while (TimelineExists(FinalName))
		{
			FinalName = FName(*FString::Printf(TEXT("%s_%d"), *TimelineName, Suffix++));
		}
	}

	// Create backing UTimelineTemplate first, then create node referencing it
	UTimelineTemplate* TimelineTemplate = FBlueprintEditorUtils::AddNewTimeline(Blueprint, FinalName);
	if (!TimelineTemplate)
	{
		OutError = FString::Printf(TEXT("Failed to create timeline '%s'"), *TimelineName);
		return nullptr;
	}

	FGraphNodeCreator<UK2Node_Timeline> NodeCreator(*Graph);
	UK2Node_Timeline* TimelineNode = NodeCreator.CreateNode();
	TimelineNode->TimelineName = FinalName;
	TimelineNode->NodePosX = PosX;
	TimelineNode->NodePosY = PosY;
	NodeCreator.Finalize();

	return TimelineNode;
}
