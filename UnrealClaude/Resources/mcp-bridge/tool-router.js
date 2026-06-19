/**
 * Tool Router for UE5 MCP Bridge
 *
 * Classifies tools into three layers:
 * - Simple: pass through from Unreal unchanged (12 tools)
 * - Hidden: callable but never listed (9 tools)
 * - Mega: collapsed into unreal_ue router (7 tools)
 *
 * Token budget: 28 tools / ~30K tokens -> 16 tools / ~12K tokens
 */

// Simple tools: appear in list_tools with full schema
export const SIMPLE_TOOL_NAMES = new Set([
  "spawn_actor",
  "move_actor",
  "delete_actors",
  "set_property",
  "get_property",       // Sprint 1: read actor property
  "get_level_actors",
  "open_level",
  "niagara",            // Sprint 3: Niagara particle operations
  "level",              // Sprint 3: level save/bounds/select/focus
  "asset_search",
  "asset_dependencies",
  "asset_referencers",
  "capture_viewport",
  "get_output_log",
  "blueprint_query",
  "execute_script",     // exposed for direct Python/C++/console execution via MCP
]);

// Hidden tools: callable but never listed
export const HIDDEN_TOOL_NAMES = new Set([
  "task_submit",
  "task_status",
  "task_result",
  "task_list",
  "task_cancel",
  "cleanup_scripts",
  "get_script_history",
  "run_console_command",
]);

// Domain -> underlying Unreal tool name
export const DOMAIN_TOOL_MAP = {
  blueprint: "blueprint_modify",
  anim: "anim_blueprint_modify",
  character: "character",
  enhanced_input: "enhanced_input",
  material: "material",
  asset: "asset",
  widget: "widget_blueprint",
};

// Blueprint operations that route to "blueprint_query" instead of "blueprint_modify"
export const BLUEPRINT_QUERY_OPS = new Set([
  "list",
  "inspect",
  "get_graph",
  "get_nodes",
  "get_variables",
  "get_functions",
  "get_node_pins",
  "search_nodes",
  "find_references",
  "find_function",        // Sprint 1: search UFUNCTIONs by name across engine classes
  "get_class_functions",  // Sprint 3: list all BlueprintCallable functions on a class
]);

// Character operations that route to "character_data" instead of "character".
// Names must match the Operation strings dispatched in MCPTool_CharacterData.cpp.
const CHARACTER_DATA_OPS = new Set([
  "create_character_data",
  "query_character_data",
  "get_character_data",
  "update_character_data",
  "create_stats_table",
  "query_stats_table",
  "add_stats_row",
  "update_stats_row",
  "remove_stats_row",
  "apply_character_data",
]);

/**
 * Resolve a router call to the underlying Unreal tool name.
 * @param {string} domain - e.g. "blueprint", "anim", "character"
 * @param {string} operation - e.g. "add_variable", "create_state_machine"
 * @returns {string|null} Underlying tool name, or null if domain unknown
 */
export function resolveUnrealTool(domain, operation) {
  if (!domain) return null;
  if (domain === "character" && CHARACTER_DATA_OPS.has(operation)) {
    return "character_data";
  }
  if (domain === "blueprint" && BLUEPRINT_QUERY_OPS.has(operation)) {
    return "blueprint_query";
  }
  return DOMAIN_TOOL_MAP[domain] ?? null;
}

/**
 * Classify a tool for list_tools filtering.
 * @param {string} toolName - raw Unreal tool name (no "unreal_" prefix)
 * @returns {"simple"|"hidden"|"mega"}
 */
export function classifyTool(toolName) {
  if (SIMPLE_TOOL_NAMES.has(toolName)) return "simple";
  if (HIDDEN_TOOL_NAMES.has(toolName)) return "hidden";
  return "mega";
}

// Reverse map: tool name → domain (built from DOMAIN_TOOL_MAP)
const TOOL_TO_DOMAIN = Object.fromEntries(
  Object.entries(DOMAIN_TOOL_MAP).map(([domain, tool]) => [tool, domain])
);
TOOL_TO_DOMAIN["character_data"] = "character"; // sub-route

/**
 * Categorize a tool for the unreal_status health check.
 * Uses the router classification + domain map for accurate grouping.
 * @param {string} toolName - raw Unreal tool name (no "unreal_" prefix)
 * @returns {string} Category name for status display
 */
export function categorizeToolForStatus(toolName) {
  const cls = classifyTool(toolName);
  if (cls === "mega") return TOOL_TO_DOMAIN[toolName] || "utility"; // widget_blueprint → "widget" via TOOL_TO_DOMAIN
  if (cls === "hidden") return toolName.startsWith("task_") ? "task_queue" : "scripting";
  // Simple tools
  if (toolName === "execute_script") return "scripting"; // simple now, but report as scripting
  if (toolName.startsWith("asset_")) return "asset";
  if (toolName === "blueprint_query") return "blueprint";
  if (toolName === "open_level" || toolName === "level") return "level";
  if (toolName === "niagara") return "vfx";
  if (toolName.includes("actor") || toolName === "spawn_actor" ||
      toolName === "move_actor" || toolName === "delete_actors" ||
      toolName === "set_property" || toolName === "get_property") return "actor";
  return "utility"; // capture_viewport, get_output_log
}

/**
 * Static MCP schema for the unreal_ue router tool.
 */
export const ROUTER_TOOL_SCHEMA = {
  name: "unreal_ue",
  description: [
    "Route a command to a domain-specific Unreal Editor tool.",
    "",
    'domain:"blueprint"',
    "  modify ops: create, add_variable, remove_variable, set_variable_default,",
    "  add_function, remove_function, add_node, add_nodes, delete_node,",
    "  connect_pins, disconnect_pins, bulk_connect, set_pin_value",
    "  query ops: list, inspect, get_graph, get_nodes, get_variables,",
    "  get_functions, get_node_pins, search_nodes, find_references,",
    "  find_function, get_class_functions",
    "  Modify requires blueprint_path. Query: list uses path_filter/type_filter/name_filter.",
    "  inspect/get_graph/get_nodes/get_variables/get_functions require blueprint_path.",
    "  get_node_pins requires blueprint_path + node_id.",
    "  search_nodes requires blueprint_path + query.",
    "  find_references requires blueprint_path + ref_name.",
    "  find_function: query (substring), exact (bool), limit — returns class+CallFunction hint.",
    "  get_class_functions: class (required), include_inherited (default true), pure_only, limit.",
    "  set_variable_default: blueprint_path, variable_name, value (string).",
    "  bulk_connect: blueprint_path, connections[] (source_node_id/source_pin/target_node_id/target_pin).",
    "  add_node node_types: CallFunction, Branch, Event, CustomEvent, VariableGet/Set,",
    "  Sequence, Cast, ForEach, ForEachWithBreak, DoOnce, Gate, Delay,",
    "  SwitchInt, SwitchString, SwitchEnum (needs enum_class), MakeStruct, BreakStruct,",
    "  MakeArray, Select, Timeline, GetSubsystem, GetSubsystemFromPC,",
    "  Add, Subtract, Multiply, Divide, PrintString, EnhancedInputAction.",
    "  VariableGet/Set accept variable OR variable_name. Cast needs class/target_class.",
    "  MakeStruct/BreakStruct needs struct (FVector/FHitResult/etc). Timeline needs timeline_name.",
    "  GetSubsystem needs subsystem_class (e.g. EnhancedInputLocalPlayerSubsystem).",
    "  add_nodes per-node spec accepts type or node_type + params or node_params; connections accept",
    "  from_node/from_pin/to_node/to_pin OR source_node_id/source_pin/target_node_id/target_pin.",
    "  connect_pins/disconnect_pins/bulk_connect/delete_node/add_node/set_pin_value optionally accept",
    "  graph_name + is_function_graph (default: event graph).",
    "  No explicit compile op — modify ops auto-compile.",
    "",
    'domain:"anim" (requires params.blueprint_path)',
    "  ops: get_info, get_state_machine, create_state_machine, add_state, remove_state,",
    "  set_entry_state, add_transition, remove_transition, set_transition_duration,",
    "  set_transition_priority, add_condition_node, delete_condition_node,",
    "  connect_condition_nodes, connect_to_result, connect_state_machine_to_output,",
    "  set_state_animation, find_animations, batch, get_transition_nodes,",
    "  inspect_node_pins, set_pin_default_value (or set_pin_value), add_comparison_chain,",
    "  validate_blueprint, get_state_machine_diagram, setup_transition_conditions,",
    "  add_variable, set_variable_default, remove_variable, compile, get_states, get_transitions, get_conduits",
    "  Variable ops use 'variable_name'/'variable_type' (NOT var_name/var_type).",
    "  state_machine accepts either the bound graph name or the node_id from get_info.",
    "  Position accepts BOTH position:{x,y} (canonical) and pos_x/pos_y scalars (matches blueprint domain).",
    "  delete_condition_node/inspect_node_pins/set_pin_default_value/connect_condition_nodes/",
    "  connect_to_result all need: state_machine + from_state + to_state + node_id.",
    "  set_state_animation needs: state_machine, state_name, animation_path; optional animation_type",
    "  (sequence|blendspace|blendspace1d|montage), parameter_bindings (object).",
    "  find_animations canonical filter is 'animation_filter' ('asset_type' deprecated to avoid bridge-wide alias collision).",
    "",
    'domain:"character" (key params: blueprint_path, character_name, asset_name, table_path)',
    "  ops: list_characters, get_character_info, get_movement_params, set_movement_params,",
    "  get_components, get_character_config, assign_anim_bp,",
    "  create_character_data, query_character_data, get_character_data, update_character_data,",
    "  create_stats_table, query_stats_table, add_stats_row, update_stats_row,",
    "  remove_stats_row, apply_character_data",
    "  get_character_config + assign_anim_bp need blueprint_path; assign_anim_bp also needs anim_blueprint_path.",
    "  Character data ops use asset_path; stats table ops use table_path (different asset types).",
    "  query_character_data optional filters: search_name (substring), search_tags (array).",
    "  list_characters returns 'total_found' (and 'total' as deprecated alias).",
    "",
    'domain:"enhanced_input" (key params: action_path/context_path for mutations, action_name/context_name for friendly lookup)',
    "  ops: create_input_action, create_mapping_context, add_mapping, remove_mapping,",
    "  add_trigger, add_modifier, query_context, query_action,",
    "  list_actions, list_contexts, get_action_info",
    "  Mutations + query_* canonical: action_path / context_path (full /Game/...).",
    "  query_action / query_context also accept action_name / context_name as alias.",
    "  get_action_info is the friendly-name variant of query_action.",
    "  create_input_action canonical 'action_name' (also accepts 'name'); create_mapping_context canonical 'context_name' (also accepts 'name').",
    "  list_actions / list_contexts: optional package_path (default /Game/), name_pattern, limit (1-1000, default 50).",
    "  create_input_action value_type accepts: Digital|Boolean|Bool, Axis1D|Float, Axis2D|Vector2D, Axis3D|Vector.",
    "",
    'domain:"material" (key params: material_path, actor_name, parent_material, asset_name)',
    "  ops: create_material_instance, set_material_parameters,",
    "  set_skeletal_mesh_material, set_actor_material, get_material_info",
    "  Canonical path key across the domain is 'material_path'.",
    "  set_material_parameters accepts 'material_instance_path' as deprecated alias.",
    "  get_material_info accepts 'asset_path' as deprecated alias.",
    "",
    'domain:"asset" (key params: asset_path, or asset_name+package_path for create)',
    "  ops: set_asset_property, save_asset, get_asset_info, list_assets,",
    "  duplicate, rename, delete, move, reimport",
    "  duplicate uses 'destination_path' (full target asset path).",
    "  move uses 'destination_directory' (asset name preserved); also accepts 'destination_path' as alias.",
    "  list_assets uses 'directory'; also accepts 'path_filter' as alias (matches asset_search).",
    "",
    'domain:"widget" (key params: blueprint_path, widget_name, widget_type)',
    "  ops: create, add_widget, query, inspect_slot, set_slot",
    "  create: blueprint_path (name or full /Game/... path), optional package_path (default /Game/UI/), parent_class.",
    "  Creates WBP with CanvasPanel root; auto-creates BindWidget children if parent_class has them.",
    "  add_widget: blueprint_path, widget_name, widget_type, optional parent_widget.",
    "  widget_type values: TextBlock, Button, Image, ProgressBar, Slider, CheckBox, EditableText,",
    "  RichTextBlock, Border, Canvas, HorizontalBox, VerticalBox, GridPanel, Overlay, SizeBox, ScaleBox, Spacer, Throbber.",
    "  query: blueprint_path — returns widget hierarchy (name, class, parent, child_count).",
    "  inspect_slot: blueprint_path, widget_name — returns slot layout props (anchors/offsets/alignment for Canvas; padding/align/size for HBox/VBox).",
    "  set_slot: blueprint_path, widget_name + layout fields — mutates slot, recompiles, saves.",
    "  Canvas slot fields: anchor_min{x,y}, anchor_max{x,y}, alignment{x,y}, offsets{left,top,right,bottom}, auto_size, z_order.",
    "  HBox/VBox slot fields: padding{left,top,right,bottom}, h_align (left|center|right|fill), v_align (top|center|bottom|fill), size_rule (auto|fill), fill_value.",
    "",
    "Pass all domain-specific params inside the params object.",
  ].join("\n"),
  inputSchema: {
    type: "object",
    required: ["domain", "operation"],
    properties: {
      domain: {
        type: "string",
        description: "blueprint | anim | character | enhanced_input | material | asset | widget",
        // Note: niagara, level, get_property, set_property, spawn_actor etc. are direct simple tools — call them without unreal_ue router
      },
      operation: {
        type: "string",
        description: "The specific operation to perform within the domain",
      },
      params: {
        type: "object",
        description: "All domain-specific parameters as key-value pairs",
      },
    },
  },
  annotations: {
    readOnlyHint: false,
    destructiveHint: true,
    idempotentHint: false,
    openWorldHint: false,
  },
};
