# UnrealClaude

![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.7-313131?style=flat&logo=unrealengine&logoColor=white)
![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat&logo=c%2B%2B&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Win64%20%7C%20Linux%20%7C%20Mac-0078D6?style=flat&logo=windows&logoColor=white)
![Claude Code](https://img.shields.io/badge/Claude%20Code-Integration-D97757?style=flat&logo=anthropic&logoColor=white)
![MCP](https://img.shields.io/badge/MCP-20%2B%20Tools-8A2BE2?style=flat)
![License](https://img.shields.io/badge/License-MIT-green?style=flat)

**Claude Code CLI integration for Unreal Engine 5.7** - Get AI coding assistance with built-in UE5.7 documentation context directly in the editor.

**Claude Code Plan changes PLEASE READ BEFORE JUNE 15TH TOO KEEP USING** 

[Changes annouced by Anthropic](https://support.claude.com/en/articles/15036540-use-the-claude-agent-sdk-with-your-claude-plan) might lead to extra usage billage when using the **custom in-editor chat box** . 
Because the plugin launches a MCP server on editor boot, **you can keep using UnrealClaude in claude code with the '/mcp' command.** 

> **Supported Platforms:** Windows (Win64), Linux, and macOS (Apple Silicon). Claude Opus 4.7 and its Claude Code release supported.
## Overview

UnrealClaude integrates the [Claude Code CLI](https://docs.anthropic.com/en/docs/claude-code) directly into the Unreal Engine 5.7 Editor and runs a MCP server for access to other coding agents.

<p align="center">
  <img width="45%" height="400" alt="Screenshot in editor chat" src="https://github.com/user-attachments/assets/5eff6f0d-8900-485c-b692-141bfb45d397" />
  <img width="45%" height="500" alt="Screenshot claude code" src="https://github.com/user-attachments/assets/abcd41ce-7caf-4e7b-87b8-39a406d29cd5" />
</p>


**Key Features:**
- **Native Editor Integration** - Chat panel docked in your editor with live streaming responses, tool call grouping, and code block rendering
- **MCP Server** - 20+ Model Context Protocol tools for actor manipulation, Blueprint editing, level management, materials, input, and more
- **Dynamic UE 5.7 Context System** - The MCP bridge includes a dynamic context loader that provides accurate UE 5.7 API documentation on demand
- **Blueprint Editing** - Create and modify Blueprints, Animation Blueprints, state machines (Few bugs still, don't rely on fully)
- **Level Management** - Open, create, and manage levels and map templates programmatically
- **Asset Management** - Search assets, query dependencies and referencers
- **Async Task Queue** - Long-running operations won't timeout
- **Script Execution** - Claude can write, compile (via Live Coding), and execute scripts with your permission
- **Session Persistence** - Conversation history saved across editor sessions
- **Project-Aware** - Automatically gathers project context (modules, plugins, assets) and is able to see editor viewports
- **Uses Claude Code Auth** - No separate API key management needed

## Prerequisites

### 1. Install Claude Code CLI

```bash
npm install -g @anthropic-ai/claude-code
```

### 2. Authenticate Claude Code

```bash
claude auth login
```

This will open a browser window to authenticate with your Anthropic account (Claude Pro/Max subscription) or set up API access.

### 3. Verify Installation

```bash
claude --version
claude -p "Hello, can you see me?"
```

## Installation

<img width="1222" height="99" alt="Screenshot 2026-02-06 112433" src="https://github.com/user-attachments/assets/61d72364-f7bc-4f34-a768-aedc0f5cea2e" />

(Check the Editor category in the plugin browser. You might need to scroll down for it if search doesn't pick it up)

### Step 1: Clone and Build

This plugin must be built from source for your platform and engine version. No prebuilt binaries are included.

1. Clone this repository (includes the MCP bridge submodule):
   ```bash
   git clone --recurse-submodules https://github.com/Natfii/UnrealClaude.git
   ```
   If you already cloned without `--recurse-submodules`, run:
   ```bash
   cd UnrealClaude
   git submodule update --init
   ```

2. Build the plugin:

   **Windows:**
   ```bash
   Engine\Build\BatchFiles\RunUAT.bat BuildPlugin -Plugin="PATH\TO\UnrealClaude\UnrealClaude\UnrealClaude.uplugin" -Package="OUTPUT\PATH" -TargetPlatforms=Win64
   ```

   **Linux:**
   ```bash
   Engine/Build/BatchFiles/RunUAT.sh BuildPlugin -Plugin="/path/to/UnrealClaude/UnrealClaude/UnrealClaude.uplugin" -Package="/output/path" -TargetPlatforms=Linux
   ```

   **macOS:**
   ```bash
   Engine/Build/BatchFiles/RunUAT.sh BuildPlugin -Plugin="/path/to/UnrealClaude/UnrealClaude/UnrealClaude.uplugin" -Package="/output/path" -TargetPlatforms=Mac
   ```

### Step 2: Install the Plugin

Copy the built plugin to either your **project** or **engine** plugins folder.

**Option A: Project Plugin (Recommended)**

Copy the build output to your project's `Plugins` directory:
```
YourProject/
├── Content/
├── Source/
└── Plugins/
    └── UnrealClaude/
        ├── Binaries/
        ├── Source/
        ├── Resources/
        ├── Config/
        └── UnrealClaude.uplugin
```

**Option B: Engine Plugin (All Projects)**

Copy to your engine's plugins folder:

**Windows:**
```
C:\Program Files\Epic Games\UE_5.7\Engine\Plugins\Marketplace\UnrealClaude\
```

**Linux:**
```
/path/to/UnrealEngine/Engine/Plugins/Marketplace/UnrealClaude/
```

### Step 3: Install MCP Bridge Dependencies

Required for Blueprint tools and editor integration:
```bash
cd <PluginPath>/UnrealClaude/Resources/mcp-bridge
npm install
```

### Step 4: Launch

Launch the editor - the plugin will load automatically.


### Step 5: (Optional but recommend): Connect Claude Code CLI to use Unreal without the chat box 

1. **Launch Claude Code or Claude CLI** and point it to your UnrealClaude install. 

<img width="1097" height="154" alt="image" src="https://github.com/user-attachments/assets/05a88a96-abce-4ebb-93ef-1ea405e5f28a" />


2. After the all clear, **restart just Claude Code / CLI for changes to save.** 

<p align="center">
  <img height="200" alt="Screenshot 1" src="https://github.com/user-attachments/assets/251f1580-aec2-415c-abab-6a2e50662a8e" />
  <img height="200" alt="Screenshot 2" src="https://github.com/user-attachments/assets/ffda3096-d808-41e7-85ba-176dfe6385d8" />
</p>



## macOS Quick Start (Apple Silicon)

For full details, see [INSTALL_MAC.md](INSTALL_MAC.md).

1. **Install Node.js and Claude Code CLI:**
   ```bash
   brew install node
   npm install -g @anthropic-ai/claude-code
   claude
   ```
2. **Install the plugin** into your project's `Plugins/` directory
3. **Install MCP bridge dependencies:**
   ```bash
   cd YourProject/Plugins/UnrealClaude/Resources/mcp-bridge
   npm install
   ```
4. **Launch** the editor and open **Tools > Claude Assistant**

## Linux Quick Start (Rocky/Fedora)

For full details, see [INSTALL_LINUX.md](INSTALL_LINUX.md).

1. **Install Libraries:**
   ```bash
   sudo dnf install -y nss nspr mesa-libgbm libXcomposite libXdamage libXrandr alsa-lib pciutils-libs libXcursor atk at-spi2-atk pango cairo gdk-pixbuf2 gtk3
   ```
2. **Install Clipboard Support:**
   ```bash
   sudo dnf install -y wl-clipboard   # Wayland
   sudo dnf install -y xclip          # X11 fallback
   ```
3. **Setup Wayland:**
   ```bash
   export SDL_VIDEODRIVER=wayland
   export UE_Linux_EnableWaylandNative=1
   ```
4. **Build and Launch:**
   ```bash
   ./UnrealEditor -vulkan
   ```

## Usage

### Opening the Claude Panel

 Menu → Tools → Claude Assistant

<img width="580" height="340" alt="{778C8E0B-C354-4AD1-BBFF-B514A4D5FC16}" src="https://github.com/user-attachments/assets/2087ef40-9791-4ad9-933b-2c64370344e8" />


### Example Prompts

```
How do I create a custom Actor Component in C++?

What's the best way to implement a health system using GAS?

Explain World Partition and how to set up streaming for an open world.

Write a BlueprintCallable function that spawns particles at a location.

How do I properly use TObjectPtr<> vs raw pointers in UE5.7?
```

### Input Shortcuts

| Shortcut | Action |
|----------|--------|
| `Enter` | Send message |
| `Shift+Enter` | New line in input |
| `Escape` | Cancel current request |

## Features

### Session Persistence

Conversations are automatically saved to your project's `Saved/UnrealClaude/` directory and restored when you reopen the editor. The plugin maintains conversation context across sessions.

### Project Context

UnrealClaude automatically gathers information about your project:
- Source modules and their dependencies
- Enabled plugins
- Project settings
- Recent assets
- Custom CLAUDE.md instructions

### Scripting

<img width="707" height="542" alt="{AB6AC101-4A4C-4607-BFB6-187D49F5E65B}" src="https://github.com/user-attachments/assets/e0c2e398-8fcd-4ac6-ade7-d50870215ec1" />

### MCP Server

The plugin includes a Model Context Protocol (MCP) server with 20+ tools that expose editor functionality to Claude and external tools. The MCP server runs on port 3000 by default and starts automatically when the editor loads.

**Tool Categories:**
- **Actor Tools** - Spawn, move, delete, inspect, and set properties on actors
- **Level Management** - Open levels, create new levels from templates, list available templates
- **Blueprint Tools** - Create and modify Blueprints (variables, functions, nodes, pins)
- **Animation Blueprint Tools** - Full state machine editing (states, transitions, conditions, batch operations)
- **Asset Tools** - Search assets, query dependencies and referencers with pagination
- **Character Tools** - Character configuration, movement settings, and data queries
- **Material Tools** - Material and material instance operations
- **Enhanced Input Tools** - Input action and mapping context management
- **Utility Tools** - Console commands, output log, viewport capture, script execution
- **Async Task Queue** - Background execution for long-running operations

For full MCP tool documentation with parameters, examples, and API details, see [UnrealClaude's MCP Bridge](https://github.com/Natfii/ue5-mcp-bridge) repository.

### Complete MCP Tool Reference

The bridge exposes **~43 backend tools** to the editor, surfaced to the AI client as **~18 entries**: simple tools listed directly, long-running/scripting tools kept hidden (callable but unlisted), and six modify domains collapsed behind a single `unreal_ue` router to save context tokens. Read the live engine version and tool counts at runtime with `unreal_status` (do not assume the engine version).

#### Simple tools — called directly (`unreal_` prefix)

| Tool | What it does | Class |
|------|--------------|-------|
| `unreal_status` | Check editor connection; reports project, engine version, tool counts, context category count. Call first. | read |
| `unreal_get_ue_context` | Load UE API docs by `category` or keyword `query` (`{}` lists categories). Prefer over web search for engine API. | read |
| `unreal_open_level` | Open / create (from template) / list level maps. **Invalidates all actor refs — run alone.** | sequential |
| `unreal_spawn_actor` | Spawn an actor by class with name + location/rotation/scale. | per-object |
| `unreal_get_level_actors` | List actors in the current level (filters: `class_filter`, `name_filter`; pagination; `brief` for transforms). | read |
| `unreal_set_property` | Set an actor property via dot-path, e.g. `LightComponent.Intensity`. | per-object |
| `unreal_get_property` | Read an actor property via dot-path (complement of set_property). | read |
| `unreal_move_actor` | Move / rotate / scale an actor. | per-object |
| `unreal_delete_actors` | Delete actors from the level. **Run alone.** | sequential |
| `unreal_level` | Level ops: `save` (sequential), `get_actor_bounds` (read), `select_actors`, `focus_viewport`. | mixed |
| `unreal_asset_search` | Find assets by `class_filter`, `name_pattern`, path (paginated). | read |
| `unreal_asset_dependencies` | List assets a given asset depends on. | read |
| `unreal_asset_referencers` | List assets that reference a given asset. | read |
| `unreal_capture_viewport` | Screenshot the active viewport. | read |
| `unreal_get_output_log` | Recent output-log entries. | read |
| `unreal_blueprint_query` | Read Blueprints — ops: `list`, `inspect`, `get_graph`, `get_nodes`, `get_variables`, `get_functions`, `get_node_pins`, `search_nodes`, `find_references`, `find_function`, `get_class_functions`. | read |
| `unreal_niagara` | Niagara VFX: `spawn_system`, `set_parameter`, `list_systems`. | per-object |

#### Hidden tools — callable but not listed

| Tool | What it does | Class |
|------|--------------|-------|
| `unreal_task_submit` | Queue any MCP tool for async background execution (avoids timeout on long ops). | — |
| `unreal_task_status` | Poll status of a submitted task. | read |
| `unreal_task_result` | Fetch the result of a completed task. | read |
| `unreal_task_list` | List all async tasks. | read |
| `unreal_task_cancel` | Cancel a running task. | — |
| `unreal_execute_script` | Run C++ / Python / console scripts (10-min timeout; may prompt permission dialog). **Run alone.** | sequential |
| `unreal_cleanup_scripts` | Remove generated scripts. **Run alone.** | sequential |
| `unreal_get_script_history` | Script execution history. | read |
| `unreal_run_console_command` | Run an editor console command (dangerous commands blocked). **Run alone.** | sequential |

#### `unreal_ue` router — modify domains (`{ domain, operation, params }`)

Domain mutations **must** go through this router; never call `blueprint_modify` / `anim_blueprint_modify` / `character` / `enhanced_input` / `material` / `asset` directly. All domain args go inside `params`. Modify ops auto-compile (no explicit compile step).

| Domain | Operations |
|--------|-----------|
| `blueprint` | `create`, `add_variable`, `remove_variable`, `set_variable_default`, `add_function`, `remove_function`, `add_node`, `add_nodes`, `delete_node`, `connect_pins`, `disconnect_pins`, `bulk_connect`, `set_pin_value` (+ all query ops, also reachable via `unreal_blueprint_query`). 30+ `add_node` types: CallFunction, Branch, Event, CustomEvent, VariableGet/Set, Sequence, Cast, ForEach, ForEachWithBreak, DoOnce, Gate, Delay, SwitchInt/String/Enum, MakeStruct, BreakStruct, MakeArray, Select, Timeline, GetSubsystem, GetSubsystemFromPC, Add/Subtract/Multiply/Divide, PrintString, EnhancedInputAction. |
| `anim` | `get_info`, `get_state_machine`, `create_state_machine`, `add_state`, `remove_state`, `set_entry_state`, `add_transition`, `remove_transition`, `set_transition_duration`, `set_transition_priority`, `add_condition_node`, `delete_condition_node`, `connect_condition_nodes`, `connect_to_result`, `connect_state_machine_to_output`, `set_state_animation`, `find_animations`, `batch`, `get_transition_nodes`, `inspect_node_pins`, `set_pin_default_value`, `add_comparison_chain`, `validate_blueprint`, `get_state_machine_diagram`, `setup_transition_conditions`, `add_variable`, `set_variable_default`, `remove_variable`, `compile`, `get_states`, `get_transitions`, `get_conduits`. |
| `character` | `list_characters`, `get_character_info`, `get_movement_params`, `set_movement_params`, `get_components`, `get_character_config`, `assign_anim_bp`, plus DataAsset/DataTable ops: `create_character_data`, `query_character_data`, `get_character_data`, `update_character_data`, `create_stats_table`, `query_stats_table`, `add_stats_row`, `update_stats_row`, `remove_stats_row`, `apply_character_data`. |
| `enhanced_input` | `create_input_action`, `create_mapping_context`, `add_mapping`, `remove_mapping`, `add_trigger`, `add_modifier`, `query_context`, `query_action`, `list_actions`, `list_contexts`, `get_action_info`. |
| `material` | `create_material_instance`, `set_material_parameters`, `set_skeletal_mesh_material`, `set_actor_material`, `get_material_info`. |
| `asset` | `set_asset_property`, `save_asset`, `get_asset_info`, `list_assets`, `duplicate`, `rename`, `delete`, `move`, `reimport`. |

#### Concurrency rules

The Unreal task queue runs **max 4 concurrent tasks**. Keep parallel subagents to **3 max** (one slot for the lead) and **≤8 sequential calls per subagent**. Timeout chain: 30 s game-thread dispatch → 2 min per task → 5 min bridge async → 10 min for `execute_script`. Read-only tools parallelize freely; modify tools parallelize only across **different** objects; sequential-only tools (`open_level`, `delete_actors`, `execute_script`, `cleanup_scripts`, `run_console_command`, `level save`) must run alone. Env overrides: `MCP_REQUEST_TIMEOUT_MS`, `MCP_ASYNC_TIMEOUT_MS`, `MCP_POLL_INTERVAL_MS`, `INJECT_CONTEXT`.

#### Dynamic UE 5.7 Context System

The MCP bridge includes a dynamic context loader that provides accurate UE 5.7 API documentation on demand. Use `unreal_get_ue_context` to query by category (animation, blueprint, slate, actor, assets, replication) or search by keywords. Context status is shown in `unreal_status` output.

## Configuration

### Project Settings

In **Project Settings → Plugins → Unreal Claude**:

- **Auto-approve script execution** (default: OFF) — when ON, every Python / C++ / Console / Editor Utility script triggered through the MCP bridge or the in-editor chat runs immediately without showing the permission dialog. Each auto-approved script writes a `LogUnrealClaude` Log entry with its type and description so the audit trail is preserved. Designed for trusted MCP-driven / agent-driven workflows where the per-script confirmation becomes dominant friction; **only enable on machines and projects where the connected client is trusted**.

The setting is persisted to `Config/DefaultEditor.ini` under `[/Script/UnrealClaude.UnrealClaudeSettings]`.

### Custom System Prompts

You can extend the built-in UE5.7 context by creating a `CLAUDE.md` file in your project root:

```markdown
# My Project Context

## Architecture
- This is a multiplayer survival game
- Using Dedicated Server model
- GAS for all abilities

## Coding Standards
- Always use UPROPERTY for Blueprint access
- Prefix interfaces with I (IInteractable)
- Use GameplayTags for ability identification
```

### Allowed Tools

By default, the plugin runs Claude with these tools: `Read`, `Write`, `Edit`, `Grep`, `Glob`, `Bash`. You can modify this in `ClaudeSubsystem.cpp`:

```cpp
Config.AllowedTools = { TEXT("Read"), TEXT("Grep"), TEXT("Glob") }; // Read-only
```

## How It Works

1. User enters a prompt in the editor widget
2. Plugin builds context from UE5.7 knowledge + project information
3. Executes: `claude -p --skip-permissions --append-system-prompt "..." "your prompt"`
4. Claude Code runs with your project as the working directory
5. Response is captured and displayed in the chat panel
6. Conversation is persisted for future sessions

### Command Line Equivalent

```bash
cd "C:\YourProject"
claude -p --skip-permissions \
  --allowedTools "Read,Write,Edit,Grep,Glob,Bash" \
  --append-system-prompt "You are an expert Unreal Engine 5.7 developer..." \
  "How do I create a custom GameMode?"
```

## Troubleshooting

### "Claude CLI not found"

1. Verify Claude is installed: `claude --version`
2. Check it's in your PATH: `where claude`
3. Restart Unreal Editor after installation

### "Authentication required"

Run `claude auth login` in a terminal to authenticate.

### Responses are slow

Claude Code executes in your project directory and may read files for context. Large projects may have slower initial responses.

You might also have too many global Claude Code plugins enabled (i.e. Superpowers, ralp-loop, context7). The context for those plugins 
getting injected can cause slowdowns up to 3+ minutes. 

### Plugin doesn't compile

Ensure you're on Unreal Engine 5.7. Supported platforms are Windows (Win64), Linux, and macOS.

### MCP Server not starting

Check if port 3000 is available. The MCP server logs to `LogUnrealClaude`.

### MCP tools not available / Blueprint tools not working

If Claude says the MCP tools are in its instructions but not in its function list:

1. **Install MCP bridge dependencies**: The most common cause is missing npm packages:
   ```bash
   cd YourProject/Plugins/UnrealClaude/Resources/mcp-bridge
   npm install
   ```

2. **Verify the HTTP server is running**: With the editor open, test:
   ```bash
   curl http://localhost:3000/mcp/status
   ```
   You should see a JSON response with project info.

3. **Check the Output Log**: Look for `LogUnrealClaude` messages:
   - `MCP Server started on http://localhost:3000` - Server is running
   - `Registered X MCP tools` - Tools are loaded

4. **Restart the editor**: After installing npm dependencies, restart Unreal Editor.

### Request Hangs 

New watchdog should warn about hung requests (60s). Make sure all files are on disk (DISABLE OneDrive, DropBox, etc.)

### Debugging the MCP Bridge

The MCP bridge is also available as a [standalone repository](https://github.com/Natfii/ue5-mcp-bridge) with its own Vitest test suite. If you're experiencing bridge-level issues (tool listing, parameter translation, context injection), you can run the bridge tests independently:

```bash
cd path/to/ue5-mcp-bridge
npm install
npm test
```

This tests the bridge without requiring a running Unreal Editor.


## Contributing

Feel free to fork for your own needs! Possible areas for improvement:

- [x] Linux support (thanks [@bearyjd](https://github.com/bearyjd))
- [x] Mac support (thanks [@lateralsummer](https://github.com/lateralsummer))
- [ ] Additional MCP tools (current tools need refractoring, no new ones for now)

## License

MIT License - See [LICENSE](UnrealClaude/LICENSE) file.

## Credits

- Built for Unreal Engine 5.7
- Integrates with [Claude Code](https://claude.ai/code) by Anthropic
