# UE5 MCP Bridge

[![Tests](https://img.shields.io/badge/tests-202%20passed-brightgreen)](tests/) [![Node](https://img.shields.io/badge/node-%3E%3D18-blue)](package.json) [![License: MIT](https://img.shields.io/badge/license-MIT-yellow)](LICENSE)

Node.js MCP (Model Context Protocol) server that bridges MCP-compatible AI clients to the Unreal Editor over HTTP. Part of the [UnrealClaude](../../..) plugin — the C++ plugin runs the HTTP backend (default `http://localhost:3000`), this `index.js` translates MCP stdio ↔ that REST API.

Targets UE 5.x — read the live engine version at runtime from `unreal_status`.

> **Setup, build, and MCP-client configuration live in the [root README](../../../README.md).** This file documents the bridge's tools and behavior. Dependencies: `npm install` in this directory (Node ≥ 18).

---

## How it works

The plugin registers ~43 backend tools. To keep the AI client's context small, the bridge classifies them:

- **Simple** — listed directly with full schema, called as `unreal_<tool>`.
- **Hidden** — callable by name but not listed (async task queue, scripting).
- **Mega** — six modify domains collapsed behind a single **`unreal_ue` router**.

So the client sees ~18 entries, not 43. The bridge also **auto-queues** every modifying call as an async task and polls until done (read-only tools run synchronously), so long operations don't hit the request timeout. The tool list is cached (`MCP_TOOL_CACHE_TTL_MS`, default 30 s).

REST endpoints consumed on the plugin: `GET /mcp/status`, `GET /mcp/tools`, `POST /mcp/tool/{name}`.

---

## The `unreal_ue` router

Domain mutations go through one tool: `unreal_ue` with `{ domain, operation, params }`. Do **not** call the underlying `*_modify` tools directly — they are not exposed.

```json
{ "domain": "blueprint", "operation": "add_variable",
  "params": { "blueprint_path": "/Game/BP_Player", "var_name": "Health", "var_type": "float" } }
```

| Domain | Underlying tool | Covers |
|--------|-----------------|--------|
| `blueprint` | `blueprint_modify` (+ `blueprint_query` for reads) | variables, functions, nodes, pins |
| `anim` | `anim_blueprint_modify` | state machines, transitions, conditions, animation assignment |
| `character` | `character` / `character_data` | ACharacter actors, config DataAssets, stats DataTables |
| `enhanced_input` | `enhanced_input` | InputActions, MappingContexts, triggers, modifiers |
| `material` | `material` | material instances, parameters, slot assignment |
| `asset` | `asset` | set property, save, duplicate, rename, delete, move, reimport |

Modify ops **auto-compile** — no explicit compile step. Per-domain operation lists are in the `unreal_ue` tool description and the [Animation Blueprint Operations](#animation-blueprint-operations) section below.

---

## Tools

### Connection & Context

| Tool | Description |
|------|-------------|
| `unreal_status` | Connection state: project, engine version, tool counts, context-category count |
| `unreal_get_ue_context` | Get UE API documentation by `category` or keyword `query` |

### Level & Actor

| Tool | Description |
|------|-------------|
| `unreal_open_level` | Open, create, or list level maps (invalidates actor refs — run alone) |
| `unreal_spawn_actor` | Spawn actors in the level |
| `unreal_get_level_actors` | List actors in the current level |
| `unreal_set_property` | Set actor properties via dot-path (e.g. `LightComponent.Intensity`) |
| `unreal_get_property` | Read actor properties via dot-path |
| `unreal_move_actor` | Move/rotate/scale actors |
| `unreal_delete_actors` | Delete actors (run alone) |
| `unreal_level` | Level ops: `save`, `get_actor_bounds`, `select_actors`, `focus_viewport` |
| `unreal_run_console_command` | Run Unreal console commands (run alone) |
| `unreal_get_output_log` | Get recent output log entries |

### Asset

| Tool | Description |
|------|-------------|
| `unreal_asset_search` | Search assets by class, path, or name |
| `unreal_asset_dependencies` | Assets a given asset depends on |
| `unreal_asset_referencers` | Assets that reference a given asset |

### Blueprint

| Tool | Description |
|------|-------------|
| `unreal_blueprint_query` | **(direct, read-only)** list, inspect, get_graph, get_nodes, get_variables, get_functions, get_node_pins, search_nodes, find_references, **find_function**, **get_class_functions** |
| `unreal_ue` domain `blueprint` | **(via router)** create, add/remove variable/function, **set_variable_default**, add_node (30+ types), add_nodes, delete_node, connect_pins, **bulk_connect**, disconnect_pins, set_pin_value |

**30+ node types:** CallFunction, Branch, Event, CustomEvent, VariableGet/Set, Sequence, Cast, ForEach, ForEachWithBreak, DoOnce, Gate, Delay, SwitchInt/String/Enum (Enum needs `enum_class`), MakeStruct, BreakStruct, MakeArray, Select, Timeline (needs `timeline_name`), GetSubsystem (needs `subsystem_class`), GetSubsystemFromPC, Add/Subtract/Multiply/Divide, PrintString, EnhancedInputAction.

### Other domains (via `unreal_ue` router)

| Domain | Description |
|--------|-------------|
| `anim` | Full Animation Blueprint manipulation — see below |
| `character` | Query/modify ACharacter actors; config DataAssets, stats DataTables |
| `material` | Material instance creation, parameters, assignment |
| `enhanced_input` | Create/modify InputAction, InputMappingContext, triggers, modifiers |

### VFX, Viewport, Scripting

| Tool | Description |
|------|-------------|
| `unreal_niagara` | Niagara ops: `spawn_system`, `set_parameter`, `list_systems` |
| `unreal_capture_viewport` | Screenshot the active viewport |
| `unreal_execute_script` | Execute C++/Python/console scripts (run alone) |
| `unreal_cleanup_scripts` | Remove generated scripts |
| `unreal_get_script_history` | Script execution history |

### Async Task Queue

| Tool | Description |
|------|-------------|
| `unreal_task_submit` | Submit a backend tool (by name) for async execution |
| `unreal_task_status` | Check a task's status |
| `unreal_task_result` | Get a completed task's result |
| `unreal_task_list` | List async tasks |
| `unreal_task_cancel` | Cancel a running task |

---

## Animation Blueprint Operations

`unreal_ue` domain `anim` provides full control over Animation Blueprints. All ops require `blueprint_path`.

**State machine:** `get_info`, `get_state_machine`, `create_state_machine`, `add_state`, `remove_state`, `set_entry_state`, `connect_state_machine_to_output`, `get_states`, `get_transitions`, `get_conduits`, `get_state_machine_diagram`.

**Transitions:** `add_transition`, `remove_transition`, `set_transition_duration`, `set_transition_priority`, `setup_transition_conditions`.

**Condition graph:** `add_condition_node` (TimeRemaining, Greater, Less, And, Or, Not, GetVariable), `delete_condition_node`, `connect_condition_nodes`, `connect_to_result`, `add_comparison_chain`.

**Node/pin introspection:** `get_transition_nodes`, `inspect_node_pins`, `set_pin_default_value`, `validate_blueprint`.

**Animation assignment:** `set_state_animation` (AnimSequence / BlendSpace / BlendSpace1D / Montage), `find_animations`.

**Variables & misc:** `add_variable`, `set_variable_default`, `remove_variable`, `compile`, `batch` (execute multiple operations atomically).

---

## UE Context System

`unreal_get_ue_context` serves built-in UE 5.x API docs from `contexts/*.md` (12 categories).

| Category | Description |
|----------|-------------|
| `animation` | Animation Blueprint, state machines, transitions, UAnimInstance |
| `blueprint` | Blueprint graphs, UK2Node (30+ types), FBlueprintEditorUtils, all MCP ops |
| `slate` | Slate UI widgets, SNew/SAssignNew, layout patterns |
| `actor` | Actor spawning, components, transforms, MCP actor + level + niagara tools |
| `assets` | Asset loading, TSoftObjectPtr, FStreamableManager, async loading |
| `replication` | Network replication, RPCs, DOREPLIFETIME, OnRep |
| `enhanced_input` | Enhanced Input System, Input Actions, Mapping Contexts |
| `character` | Character movement, configuration, stats |
| `material` | Material instances, parameters, slots |
| `niagara` | Niagara VFX: UNiagaraFunctionLibrary, spawn/parameters |
| `parallel_workflows` | Multi-agent patterns, tool parallelization classes, anti-patterns |
| `ue_core` | UPROPERTY/UFUNCTION/UCLASS specifiers, class hierarchy, include paths |

```json
{ "category": "animation" }                  // load one category
{ "query": "state machine transitions" }     // keyword search
{}                                            // list categories
```

Set `INJECT_CONTEXT=true` to auto-append relevant context to tool responses.

---

## Configuration

| Environment Variable | Default | Description |
|---------------------|---------|-------------|
| `UNREAL_MCP_URL` | `http://localhost:3000` | Unreal plugin HTTP server URL |
| `MCP_REQUEST_TIMEOUT_MS` | `30000` | HTTP request timeout (ms) |
| `MCP_ASYNC_ENABLED` | `true` | Auto-queue modifying calls as async tasks (`false` forces synchronous) |
| `MCP_ASYNC_TIMEOUT_MS` | `300000` | Overall async task timeout (5 min) |
| `MCP_POLL_INTERVAL_MS` | `2000` | Async task poll interval |
| `MCP_TOOL_CACHE_TTL_MS` | `30000` | Tool-list cache TTL |
| `INJECT_CONTEXT` | `false` | Auto-inject UE context on tool responses |
| `DEBUG` | - | Enable debug logging |

---

## Tests

Vitest suite (200+ tests) validates bridge behavior without a running editor — mocks the HTTP backend, covers schema conversion, HTTP client logic, context loading, tool listing, and call routing.

```bash
npm test              # run all
npm run test:watch    # watch mode
npm run test:coverage # coverage report
```

---

## License

MIT with attribution — see [LICENSE](LICENSE). Original author: Natali Caggiano ([@Natfii](https://github.com/Natfii)).
**Fork maintainer & contributor:** Vinayak Lakhani ([@vinayak-vc](https://github.com/vinayak-vc)) — Sprint 1–4 tool additions (niagara, get_property, find_function, get_class_functions, bulk_connect, level routing), submodule-to-vendored migration, docs.

**AI pair:** Claude (Opus 4.8, Anthropic) via Claude Code — test/count fixes, documentation rewrite, repo restructuring.