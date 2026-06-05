# UnrealClaude

![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.x-313131?style=flat&logo=unrealengine&logoColor=white)
![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat&logo=c%2B%2B&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Win64%20%7C%20Linux%20%7C%20Mac-0078D6?style=flat&logo=windows&logoColor=white)
![MCP](https://img.shields.io/badge/MCP-43%20tools-8A2BE2?style=flat)
![License](https://img.shields.io/badge/License-MIT-green?style=flat)

Editor plugin that runs an **MCP (Model Context Protocol) server inside the Unreal Editor**, letting any MCP-compatible AI client (Claude Code, Claude Desktop, Cursor, etc.) drive the live editor — spawn actors, edit Blueprints and Animation Blueprints, manage levels/assets/materials, and run scripts — plus a docked in-editor chat panel backed by the Claude Code CLI.

Tools operate on **live editor state** over an HTTP REST API, not on serialized `.uasset` files on disk.

> The `.uplugin` targets EngineVersion `5.7.0`, but the code uses UE 5.x editor APIs and has been run against 5.5.x. Read the actual engine version at runtime from the `unreal_status` tool — never assume it.

---

## Architecture

```
┌──────────────────┐   stdio (MCP)   ┌──────────────────┐   HTTP REST    ┌────────────────────────┐
│  MCP AI client   │ ───────────────▶│  Node.js bridge  │ ──────────────▶│  UnrealClaude C++ plugin │
│ (Claude Code…)   │◀─────────────── │  (index.js)      │◀────────────── │  HTTP server :3000       │
└──────────────────┘   tool results  └──────────────────┘   JSON         └────────────────────────┘
                                                                             │ dispatches on game thread
                                                                             ▼
                                                                   FMCPToolRegistry → 43 tools
                                                                   via FMCPTaskQueue (async)
```

**Three layers:**

1. **C++ editor module** (`Source/UnrealClaude/`, type `Editor`, loading phase `PostEngineInit`). On load it starts `FUnrealClaudeMCPServer` — a UE `HttpServerModule` listening on **port 3000** by default (`UnrealClaudeConstants::MCPServer::DefaultPort`). Routes:
   - `GET  /mcp/status` — connection + project + engine version
   - `GET  /mcp/tools` — tool list with JSON-schema parameters and annotations
   - `POST /mcp/tool/{name}` — execute a tool with a JSON body
   - `FMCPToolRegistry` registers **43 `FMCPToolBase` tools**. Editor-mutating work is marshalled onto the game thread and run through `FMCPTaskQueue` (async, bounded concurrency).

2. **Node.js bridge** (`UnrealClaude/Resources/mcp-bridge/`, vendored from [`ue5-mcp-bridge`](https://github.com/Natfii/ue5-mcp-bridge) and tracked directly in this repo — no submodule). A stdio MCP server (`index.js`) that translates MCP ↔ the plugin's REST API. It:
   - Caches the tool list (TTL `MCP_TOOL_CACHE_TTL_MS`, default 30 s).
   - **Classifies** every backend tool to keep the AI client's context small: *simple* (listed with full schema), *hidden* (callable but unlisted), *mega* (collapsed behind the `unreal_ue` router). Net surface: ~18 entries instead of 43.
   - **Auto-async**: wraps every non-read-only, non-`task_*` call as a queued task and polls until done — so long operations don't hit the request timeout. Read-only tools run synchronously.
   - Serves the **UE context docs** (`unreal_get_ue_context`) from `contexts/*.md`.

3. **In-editor chat** (Slate widget + `UClaudeSubsystem`). Menu → **Tools → Claude Assistant**. Shells out to the Claude Code CLI (`claude -p …`) with the project as the working directory, builds a UE-aware system prompt, and persists conversation history under `Saved/UnrealClaude/`. This path is independent of the MCP server.

---

## Tool exposure model

| Layer | Count | How the AI calls it |
|-------|-------|---------------------|
| Simple | 15 | Direct: `unreal_<tool>` with the tool's own schema |
| `unreal_status` / `unreal_get_ue_context` | 2 | Direct (always present) |
| `unreal_ue` router | 1 entry → 6 modify domains | `{ domain, operation, params }` |
| Hidden | 9 | Callable by name, not listed (task queue, scripting) |
| Advanced backend | 12 | Not routed — invoke by name via `unreal_task_submit` |

Read the live counts with `unreal_status` → `total_tools` (43) and `exposed_tools` (~18).

### Simple tools — called directly (`unreal_` prefix)

| Tool | What it does | Class |
|------|--------------|-------|
| `unreal_status` | Connection state: project, engine version, tool counts, context-category count. Call first. | read |
| `unreal_get_ue_context` | Load UE API docs by `category` or keyword `query`; `{}` lists categories. | read |
| `unreal_open_level` | Open / create-from-template / list level maps. **Invalidates all actor refs — run alone.** | sequential |
| `unreal_spawn_actor` | Spawn an actor by class with name + location/rotation/scale. | per-object |
| `unreal_get_level_actors` | List level actors (`class_filter`, `name_filter`, pagination; `brief=false` for transforms). | read |
| `unreal_set_property` | Set an actor property via dot-path, e.g. `LightComponent.Intensity`. | per-object |
| `unreal_get_property` | Read an actor property via dot-path. | read |
| `unreal_move_actor` | Move / rotate / scale an actor. | per-object |
| `unreal_delete_actors` | Delete actors. **Run alone.** | sequential |
| `unreal_level` | `save` (sequential), `get_actor_bounds` (read), `select_actors`, `focus_viewport`. | mixed |
| `unreal_asset_search` | Find assets by `class_filter`, `name_pattern`, path (paginated). | read |
| `unreal_asset_dependencies` | Assets a given asset depends on. | read |
| `unreal_asset_referencers` | Assets that reference a given asset. | read |
| `unreal_capture_viewport` | Screenshot the active viewport (returned as an image). | read |
| `unreal_get_output_log` | Recent output-log entries. | read |
| `unreal_blueprint_query` | Read Blueprints — see ops below. | read |

`unreal_blueprint_query` operations: `list`, `inspect`, `get_graph`, `get_nodes`, `get_variables`, `get_functions`, `get_node_pins`, `search_nodes`, `find_references`, `find_function`, `get_class_functions`.

### `unreal_ue` router — modify domains (`{ domain, operation, params }`)

Domain mutations **must** go through this router; never call the underlying tools directly. All domain args go inside `params`. Modify ops **auto-compile** — no explicit compile step.

| Domain | Underlying tool | Operations |
|--------|-----------------|-----------|
| `blueprint` | `blueprint_modify` (+`blueprint_query` for reads) | `create`, `add_variable`, `remove_variable`, `set_variable_default`, `add_function`, `remove_function`, `add_node`, `add_nodes`, `delete_node`, `connect_pins`, `disconnect_pins`, `bulk_connect`, `set_pin_value` |
| `anim` | `anim_blueprint_modify` | `get_info`, `get_state_machine`, `create_state_machine`, `add_state`, `remove_state`, `set_entry_state`, `add_transition`, `remove_transition`, `set_transition_duration`, `set_transition_priority`, `add_condition_node`, `delete_condition_node`, `connect_condition_nodes`, `connect_to_result`, `connect_state_machine_to_output`, `set_state_animation`, `find_animations`, `batch`, `get_transition_nodes`, `inspect_node_pins`, `set_pin_default_value`, `add_comparison_chain`, `validate_blueprint`, `get_state_machine_diagram`, `setup_transition_conditions`, `add_variable`, `set_variable_default`, `remove_variable`, `compile`, `get_states`, `get_transitions`, `get_conduits` |
| `character` | `character` / `character_data` | `list_characters`, `get_character_info`, `get_movement_params`, `set_movement_params`, `get_components`, `get_character_config`, `assign_anim_bp`, `create_character_data`, `query_character_data`, `get_character_data`, `update_character_data`, `create_stats_table`, `query_stats_table`, `add_stats_row`, `update_stats_row`, `remove_stats_row`, `apply_character_data` |
| `enhanced_input` | `enhanced_input` | `create_input_action`, `create_mapping_context`, `add_mapping`, `remove_mapping`, `add_trigger`, `add_modifier`, `query_context`, `query_action`, `list_actions`, `list_contexts`, `get_action_info` |
| `material` | `material` | `create_material_instance`, `set_material_parameters`, `set_skeletal_mesh_material`, `set_actor_material`, `get_material_info` |
| `asset` | `asset` | `set_asset_property`, `save_asset`, `get_asset_info`, `list_assets`, `duplicate`, `rename`, `delete`, `move`, `reimport` |

**`blueprint` `add_node` node types (30+):** CallFunction, Branch, Event, CustomEvent, VariableGet, VariableSet, Sequence, Cast, ForEach, ForEachWithBreak, DoOnce, Gate, Delay, SwitchInt, SwitchString, SwitchEnum (needs `enum_class`), MakeStruct, BreakStruct, MakeArray, Select, Timeline (needs `timeline_name`), GetSubsystem (needs `subsystem_class`), GetSubsystemFromPC, Add, Subtract, Multiply, Divide, PrintString, EnhancedInputAction.

### Hidden tools — callable, not listed

| Tool | What it does |
|------|--------------|
| `unreal_task_submit` | Queue any backend tool (by name) for async execution. |
| `unreal_task_status` | Poll a task's status. |
| `unreal_task_result` | Fetch a completed task's result. |
| `unreal_task_list` | List async tasks. |
| `unreal_task_cancel` | Cancel a running task. |
| `unreal_execute_script` | Run C++ / Python / console scripts (10-min timeout; permission-gated). **Run alone.** |
| `unreal_cleanup_scripts` | Remove generated scripts. **Run alone.** |
| `unreal_get_script_history` | Script execution history. |
| `unreal_run_console_command` | Run an editor console command (dangerous commands blocked). **Run alone.** |

### Advanced backend tools — invoke by name via `unreal_task_submit`

Registered in C++ but not given a router domain, so reach them through the async queue, e.g. `task_submit { tool_name: "datatable", arguments: {…} }`:

`world_settings`, `project_settings`, `build`, `set_class_defaults`, `create_cpp_class`, `import_asset`, `widget_bind`, `actor_search`, `datatable`, `curve`, `collision_profile`, `sequencer`.

These cover Live Coding builds, C++ class generation, DataTable/Curve assets, Sequencer tracks, collision profiles, UMG widget binding, and project/world settings.

---

## Concurrency & timeouts

`FMCPTaskQueue` runs **max 4 concurrent tasks**. The bridge auto-queues every modifying call.

| Stage | Limit | Env override |
|-------|-------|--------------|
| Game-thread dispatch | 30 s | — |
| Per-task | 2 min | — |
| Bridge async overall | 5 min | `MCP_ASYNC_TIMEOUT_MS` |
| Bridge poll interval | 2 s | `MCP_POLL_INTERVAL_MS` |
| HTTP request | 30 s | `MCP_REQUEST_TIMEOUT_MS` |
| `execute_script` | 10 min | — |

**Parallelization classes:**

- **Parallel-safe (read):** `asset_search`, `get_level_actors`, `blueprint_query`, `asset_dependencies`, `asset_referencers`, `capture_viewport`, `get_output_log`, `get_property`, `level get_actor_bounds`. Call freely in parallel.
- **Per-object (modify):** `spawn_actor`, `move_actor`, `set_property`, blueprint/anim/character/material/enhanced_input/asset, `niagara`, `level select_actors/focus_viewport`. Parallelize across **different** objects only — never two calls on the same object (Blueprint compilation is not thread-safe).
- **Sequential only:** `open_level`, `delete_actors`, `execute_script`, `cleanup_scripts`, `run_console_command`, `level save`. Must run alone.

Subagent guidance: **≤3 subagents** (one queue slot for the lead), **≤8 sequential calls each**; batch wider work in waves of 3–4.

---

## UE context system

`unreal_get_ue_context` serves on-demand API documentation from `Resources/mcp-bridge/contexts/*.md`. 12 categories: `animation`, `blueprint`, `slate`, `actor`, `assets`, `replication`, `enhanced_input`, `character`, `material`, `niagara`, `parallel_workflows`, `ue_core`.

```json
{ "category": "animation" }                 // load one category
{ "query": "state machine transitions" }    // keyword search
{}                                           // list categories
```

Set `INJECT_CONTEXT=true` to auto-append relevant context to tool responses.

---

## Configuration

### Bridge environment variables

| Variable | Default | Purpose |
|----------|---------|---------|
| `UNREAL_MCP_URL` | `http://localhost:3000` | Plugin HTTP server URL |
| `MCP_REQUEST_TIMEOUT_MS` | `30000` | HTTP request timeout |
| `MCP_ASYNC_ENABLED` | `true` | Auto-queue modifying calls as async tasks |
| `MCP_ASYNC_TIMEOUT_MS` | `300000` | Overall async timeout |
| `MCP_POLL_INTERVAL_MS` | `2000` | Async poll interval |
| `MCP_TOOL_CACHE_TTL_MS` | `30000` | Tool-list cache TTL |
| `INJECT_CONTEXT` | `false` | Auto-inject UE context on responses |

### Plugin setting

**Project Settings → Plugins → Unreal Claude** → *Auto-approve script execution* (`bAutoApproveScripts`, default **OFF**). When OFF, every Python/C++/Console/Editor-Utility script triggered via MCP or the chat requires a modal approval. When ON, scripts run immediately and an `INFO` audit line is logged instead. Persisted to `Config/DefaultEditor.ini` under `[/Script/UnrealClaude.UnrealClaudeSettings]`. **Only enable where the connected client is trusted.**

### Security / validation

Tools validate inputs server-side: content paths only (`/Engine/`, `/Script/`, `../` blocked), actor names reject `<>|&;$(){}[]!*?~`, dangerous console commands (quit/crash/shutdown) blocked, numeric NaN/Inf guarded. Tools carry annotations (`readOnlyHint` / `destructiveHint` / `idempotentHint`) the client uses for safety decisions.

---

## MCP client configuration

`Claude Code` (`~/.claude/settings.json`) or `Claude Desktop` (`claude_desktop_config.json`):

```json
{
  "mcpServers": {
    "unreal": {
      "command": "node",
      "args": ["/path/to/UnrealClaude/Resources/mcp-bridge/index.js"],
      "env": { "UNREAL_MCP_URL": "http://localhost:3000" }
    }
  }
}
```

With the editor open, the bridge connects automatically; in Claude Code use `/mcp` to verify. Any other MCP-compliant client works the same way (stdio transport).

---

## Prerequisites

- **Unreal Engine 5.x** (`.uplugin` declares 5.7.0; builds against 5.x editor APIs).
- **Node.js ≥ 18** for the bridge.
- **Claude Code CLI** (only for the in-editor chat panel):
  ```bash
  npm install -g @anthropic-ai/claude-code
  claude auth login
  claude --version
  ```

Module dependencies (from `UnrealClaude.Build.cs`): `HTTPServer`, `HTTP`, `Sockets`, `Networking`, `Json`, `Kismet`, `KismetCompiler`, `BlueprintGraph`, `AnimGraph`, `AnimGraphRuntime`, `AssetRegistry`, `AssetTools`, `EnhancedInput`, `InputBlueprintNodes`, `Niagara`, `NiagaraEditor`, `UMG`, `UMGEditor`, `LevelSequence`, `MovieScene`, `MovieSceneTracks`, `DeveloperSettings`, and `LiveCoding` (Win64 editor only).

---

## Build & install

This plugin must be built from source — no prebuilt binaries are shipped.

### 1. Clone

```bash
git clone https://github.com/Natfii/UnrealClaude.git
```
The MCP bridge lives at `UnrealClaude/Resources/mcp-bridge/` directly in this repo (no submodule to init).

### 2. Build the plugin

**Windows**
```bash
Engine\Build\BatchFiles\RunUAT.bat BuildPlugin -Plugin="PATH\TO\UnrealClaude\UnrealClaude\UnrealClaude.uplugin" -Package="OUTPUT\PATH" -TargetPlatforms=Win64 -Rocket
```
**Linux / macOS**
```bash
Engine/Build/BatchFiles/RunUAT.sh BuildPlugin -Plugin="/path/to/UnrealClaude/UnrealClaude/UnrealClaude.uplugin" -Package="/output/path" -TargetPlatforms=Linux   # or Mac
```

### 3. Install

Copy the build output into your project's `Plugins/UnrealClaude/` (recommended) or the engine's `Engine/Plugins/Marketplace/UnrealClaude/`.

### 4. Install bridge dependencies

```bash
cd <PluginPath>/UnrealClaude/Resources/mcp-bridge
npm install
```

### 5. Launch

Launch the editor — the plugin auto-loads and starts the MCP server. Point your MCP client at `index.js` (above) and restart the client once.

Platform-specific notes: [INSTALL_LINUX.md](INSTALL_LINUX.md), [INSTALL_MAC.md](INSTALL_MAC.md).

---

## In-editor chat

Menu → **Tools → Claude Assistant**. Streams responses, groups tool calls, renders code blocks. Shortcuts: `Enter` send, `Shift+Enter` newline, `Escape` cancel. Conversations persist in `Saved/UnrealClaude/`. Default CLI allowed tools (`Read, Write, Edit, Grep, Glob, Bash`) are set in `ClaudeSubsystem.cpp`. Extend the system prompt by adding a `CLAUDE.md` to the project root.

---

## Development

- **Source layout:** `Source/UnrealClaude/Private/MCP/` (server, registry, task queue, param validator) and `Private/MCP/Tools/` (one `MCPTool_*` per tool). Blueprint/anim editing helpers live in `Private/` (`BlueprintGraphEditor`, `AnimStateMachineEditor`, etc.).
- **Adding a tool:** subclass `FMCPToolBase`, implement `GetInfo()` (params + annotations) and `Execute()` (validate with `MCPParamValidator` helpers), register it in `MCPToolRegistry.cpp`, add tests in `Private/Tests/`.
- **Tests:** plugin automation — `Automation RunTests UnrealClaude` in the editor console. Bridge — `cd Resources/mcp-bridge && npm test` (Vitest, mocks the HTTP backend; runs without a live editor).
- **Bridge:** `UnrealClaude/Resources/mcp-bridge/` is vendored directly (not a submodule) — edit and commit it like any other path. Upstream: [ue5-mcp-bridge](https://github.com/Natfii/ue5-mcp-bridge); this copy is ahead of upstream with the Sprint 1–4 tool additions.
- **Commit prefixes:** `feat:`, `fix:`, `test:`, `docs:`, `refactor:`.

---

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| Tools listed but not callable / "NOT CONNECTED" | Editor not running, or bridge deps missing — `cd Resources/mcp-bridge && npm install`, then restart. |
| Verify server is up | `curl http://localhost:3000/mcp/status` → JSON with project info. |
| MCP server not starting | Port 3000 in use; check `LogUnrealClaude` for `MCP Server started` / `Registered X MCP tools`. |
| Requests hang | Files not flushed to disk — disable OneDrive/Dropbox sync on the project. Watchdog warns at 60 s. |
| Slow responses | Large project context, or too many global Claude Code plugins injecting context. |
| Won't compile | Confirm UE version and that all `Build.cs` module deps are available. |

---

## Credits

- **Original author:** Natali Caggiano ([@Natfii](https://github.com/Natfii)) — UnrealClaude plugin + [ue5-mcp-bridge](https://github.com/Natfii/ue5-mcp-bridge).
- **Fork maintainer & contributor:** Vinayak Lakhani ([@vinayak-vc](https://github.com/vinayak-vc)) — Sprint 1–4 MCP tool additions, UE 5.5 API compatibility, submodule-to-vendored bridge migration, technical docs.
- **AI pair:** Claude (Opus 4.8, Anthropic) via [Claude Code](https://claude.com/claude-code) — test fixes, README rewrites, repo restructuring, the `unrealclaude` skill.
- **Protocol:** [Model Context Protocol](https://modelcontextprotocol.io/).

## License

MIT with attribution — see [LICENSE](UnrealClaude/LICENSE).
