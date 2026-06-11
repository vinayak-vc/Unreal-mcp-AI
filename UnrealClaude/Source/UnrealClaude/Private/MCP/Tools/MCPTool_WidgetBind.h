// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Widget Bind / UMG Widget Blueprint
 *
 * Create and manage UMG Widget Blueprints.
 * Can auto-create widgets with named slots matching C++ BindWidget declarations.
 *
 * Operations:
 *   - create: Create a new Widget Blueprint
 *   - add_widget: Add a child widget to an existing Widget Blueprint
 *   - query: List widgets in a Widget Blueprint hierarchy
 */
class FMCPTool_WidgetBind : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("widget_blueprint");
		Info.Description = TEXT(
			"Create and manage UMG Widget Blueprints.\n\n"
			"Operations:\n"
			"- 'create': Create new Widget Blueprint with optional parent C++ class\n"
			"- 'add_widget': Add a child widget (Button, TextBlock, Image, etc.) to a WBP\n"
			"- 'query': List the widget hierarchy of a Widget Blueprint\n"
			"- 'inspect_slot': Read a widget's slot layout (anchors/alignment/offsets/padding)\n"
			"- 'set_slot': Set a widget's slot layout (CanvasPanelSlot/HBox/VBox slots)\n\n"
			"Widget types for add_widget: TextBlock, Button, Image, ProgressBar, Slider,\n"
			"CheckBox, ComboBox, EditableText, RichTextBlock, Border, Canvas, HorizontalBox,\n"
			"VerticalBox, GridPanel, Overlay, SizeBox, ScaleBox, Spacer, Throbber\n\n"
			"set_slot parameters by slot type:\n"
			"- CanvasPanelSlot: anchor_min{x,y}, anchor_max{x,y}, alignment{x,y},\n"
			"  offsets{left,top,right,bottom}, auto_size (bool), z_order (int)\n"
			"- HorizontalBoxSlot / VerticalBoxSlot: padding{left,top,right,bottom},\n"
			"  h_align ('Left'|'Center'|'Right'|'Fill'), v_align ('Top'|'Center'|'Bottom'|'Fill'),\n"
			"  size_rule ('Auto'|'Fill'), fill_value (float)\n\n"
			"When creating with a C++ parent class that has BindWidget properties,\n"
			"the tool auto-creates matching named widgets in the Blueprint."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation: 'create', 'add_widget', 'query', 'inspect_slot', or 'set_slot'"), true),
			FMCPToolParameter(TEXT("blueprint_path"), TEXT("string"),
				TEXT("Path for new or existing Widget Blueprint"), false),
			FMCPToolParameter(TEXT("widget_name"), TEXT("string"),
				TEXT("Name of the target widget (add_widget / inspect_slot / set_slot)"), false),
			FMCPToolParameter(TEXT("parent_class"), TEXT("string"),
				TEXT("C++ parent class path (for create; reads BindWidget properties)"), false),
			FMCPToolParameter(TEXT("package_path"), TEXT("string"),
				TEXT("Package path for new WBP (default: '/Game/UI/')"), false, TEXT("/Game/UI/")),
			FMCPToolParameter(TEXT("widget_type"), TEXT("string"),
				TEXT("Widget class to add (TextBlock, Button, Image, etc.)"), false),
			FMCPToolParameter(TEXT("parent_widget"), TEXT("string"),
				TEXT("Name of parent widget to nest under (for add_widget)"), false),
			FMCPToolParameter(TEXT("slot_name"), TEXT("string"),
				TEXT("Named slot to place widget in"), false),
			// --- slot layout fields (used by set_slot; see Description) ---
			FMCPToolParameter(TEXT("anchor_min"), TEXT("object"),
				TEXT("CanvasPanelSlot min anchor {x,y} (0..1)"), false),
			FMCPToolParameter(TEXT("anchor_max"), TEXT("object"),
				TEXT("CanvasPanelSlot max anchor {x,y} (0..1)"), false),
			FMCPToolParameter(TEXT("alignment"), TEXT("object"),
				TEXT("CanvasPanelSlot alignment {x,y} (0..1; 0.5,0.5 = centred)"), false),
			FMCPToolParameter(TEXT("offsets"), TEXT("object"),
				TEXT("CanvasPanelSlot offsets {left,top,right,bottom}"), false),
			FMCPToolParameter(TEXT("padding"), TEXT("object"),
				TEXT("HBox/VBox slot padding {left,top,right,bottom}"), false),
			FMCPToolParameter(TEXT("h_align"), TEXT("string"),
				TEXT("Horizontal alignment: Left|Center|Right|Fill"), false),
			FMCPToolParameter(TEXT("v_align"), TEXT("string"),
				TEXT("Vertical alignment: Top|Center|Bottom|Fill"), false),
			FMCPToolParameter(TEXT("size_rule"), TEXT("string"),
				TEXT("HBox/VBox slot size: Auto|Fill"), false),
			FMCPToolParameter(TEXT("fill_value"), TEXT("number"),
				TEXT("HBox/VBox slot fill ratio when size_rule = Fill"), false),
			FMCPToolParameter(TEXT("auto_size"), TEXT("boolean"),
				TEXT("CanvasPanelSlot auto-size (shrinks to content)"), false),
			FMCPToolParameter(TEXT("z_order"), TEXT("number"),
				TEXT("CanvasPanelSlot Z order"), false)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	FMCPToolResult ExecuteCreate(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteAddWidget(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteQuery(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteInspectSlot(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteSetSlot(const TSharedRef<FJsonObject>& Params);
};
