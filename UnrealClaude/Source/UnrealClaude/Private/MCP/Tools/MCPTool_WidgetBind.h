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
			"- 'query': List the widget hierarchy of a Widget Blueprint\n\n"
			"Widget types for add_widget: TextBlock, Button, Image, ProgressBar, Slider,\n"
			"CheckBox, ComboBox, EditableText, RichTextBlock, Border, Canvas, HorizontalBox,\n"
			"VerticalBox, GridPanel, Overlay, SizeBox, ScaleBox, Spacer, Throbber\n\n"
			"When creating with a C++ parent class that has BindWidget properties,\n"
			"the tool auto-creates matching named widgets in the Blueprint."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation: 'create', 'add_widget', or 'query'"), true),
			FMCPToolParameter(TEXT("blueprint_path"), TEXT("string"),
				TEXT("Path for new or existing Widget Blueprint"), false),
			FMCPToolParameter(TEXT("widget_name"), TEXT("string"),
				TEXT("Name for the new widget (for add_widget)"), false),
			FMCPToolParameter(TEXT("parent_class"), TEXT("string"),
				TEXT("C++ parent class path (for create; reads BindWidget properties)"), false),
			FMCPToolParameter(TEXT("package_path"), TEXT("string"),
				TEXT("Package path for new WBP (default: '/Game/UI/')"), false, TEXT("/Game/UI/")),
			FMCPToolParameter(TEXT("widget_type"), TEXT("string"),
				TEXT("Widget class to add (TextBlock, Button, Image, etc.)"), false),
			FMCPToolParameter(TEXT("parent_widget"), TEXT("string"),
				TEXT("Name of parent widget to nest under (for add_widget)"), false),
			FMCPToolParameter(TEXT("slot_name"), TEXT("string"),
				TEXT("Named slot to place widget in"), false)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	FMCPToolResult ExecuteCreate(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteAddWidget(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteQuery(const TSharedRef<FJsonObject>& Params);
};
