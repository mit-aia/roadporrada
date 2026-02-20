# Road Porrada — Project Architecture

> A 3D motorcycle street-racing/combat game inspired by _Road Rash_.

---

## Technology Stack

| Layer                 | Technology    | Role                                                                 |
| --------------------- | ------------- | -------------------------------------------------------------------- |
| **Rendering**         | bgfx          | Cross-platform, low-level GPU abstraction (D3D11/12, Vulkan, OpenGL) |
| **Windowing / Input** | GLFW 3        | Window creation, keyboard/gamepad input, event polling               |
| **Physics**           | Jolt Physics  | Rigid-body dynamics, collision detection, vehicle simulation         |
| **3D Authoring**      | Blender       | Modeling, rigging, animation, track layout                           |
| **Build**             | CMake + vcpkg | Build system and C++ dependency management                           |
| **Language**          | C++20         | Core language standard                                               |

---

## High-Level Architecture

```
┌──────────────────────────────────────────────────────┐
│                      main()                          │
│         (bootstrap, game-loop orchestration)         │
└──────────┬───────────────────────────────────────────┘
           │
     ┌─────▼─────┐
     │ GameApp   │  — owns the loop, delegates to subsystems
     └─────┬─────┘
           │
   ┌───────┼────────┬────────────┬──────────────┐
   ▼       ▼        ▼            ▼              ▼
Renderer PhysicsWorld InputMgr  SceneMgr     AudioMgr
   │       │        │            │              │
   │       │        │       ┌────┴────┐         │
   │       │        │       │  Scene  │         │
   │       │        │       │ (ECS)   │         │
   │       │        │       └─────────┘         │
   │       ▼        │                           │
   │  JoltPhysics   │                           │
   │  (rigid bodies,│                           │
   │   vehicles)    │                           │
   ▼                ▼                           ▼
  bgfx            GLFW                    (future: FMOD/miniaudio)
```

---

## File Structure

```
roadporrada/
│
├── CMakeLists.txt                  # Root CMake — defines targets, links libs
├── CMakePresets.json                # Shared CMake presets (Release, Debug, etc.)
├── CMakeUserPresets.json            # Local machine overrides (git-ignored)
├── vcpkg.json                       # vcpkg manifest (bgfx, glfw3, joltphysics)
├── vcpkg-configuration.json         # vcpkg registry / baseline config
├── .gitignore
├── ARCHITECTURE.md                  # ← this file
├── README.md
│
├── src/
│   ├── main.cpp                     # Entry point — creates GameApp, runs loop
│   │
│   ├── app/
│   │   ├── game_app.h               # GameApp class — owns subsystems, runs loop
│   │   └── game_app.cpp
│   │
│   ├── core/
│   │   ├── types.h                  # Aliases, math types (glm or bx math wrappers)
│   │   ├── timer.h / .cpp           # High-res delta-time & fixed-step timer
│   │   ├── config.h / .cpp          # Runtime settings (resolution, keybinds, etc.)
│   │   └── log.h / .cpp             # Lightweight logging (wraps printf / spdlog)
│   │
│   ├── input/
│   │   ├── input_manager.h / .cpp   # Polls GLFW, abstracts keyboard + gamepad
│   │   └── input_action.h           # Named action bindings (accelerate, punch, etc.)
│   │
│   ├── renderer/
│   │   ├── renderer.h / .cpp        # bgfx init, view setup, frame submit
│   │   ├── camera.h / .cpp          # Chase / cinematic camera
│   │   ├── mesh.h / .cpp            # Vertex/index buffer wrapper
│   │   ├── material.h / .cpp        # Shader program + uniform bundle
│   │   ├── texture.h / .cpp         # Texture loading (stb_image → bgfx)
│   │   ├── debug_draw.h / .cpp      # Wire-frame overlays (physics shapes, etc.)
│   │   └── shaders/
│   │       ├── vs_basic.sc           # bgfx shader-lang vertex shaders
│   │       ├── fs_basic.sc           # bgfx shader-lang fragment shaders
│   │       ├── varying.def.sc        # Shared varying definitions
│   │       └── build_shaders.bat     # Calls shaderc to compile .sc → .bin
│   │
│   ├── physics/
│   │   ├── physics_world.h / .cpp   # Jolt init, step, body management
│   │   ├── vehicle_body.h / .cpp    # Motorcycle rigid-body + wheel constraints
│   │   ├── collision_layers.h       # Layer / mask definitions
│   │   └── contact_listener.h/.cpp  # Impact callbacks (punch hits, crashes)
│   │
│   ├── scene/
│   │   ├── scene_manager.h / .cpp   # Loads / switches scenes (menu, race, results)
│   │   ├── entity.h                 # Lightweight entity handle (ID + generation)
│   │   ├── component.h              # Component base / tag types
│   │   ├── components/
│   │   │   ├── transform.h          # Position, rotation, scale
│   │   │   ├── renderable.h         # Mesh + material ref
│   │   │   ├── rigid_body.h         # Jolt body handle wrapper
│   │   │   ├── rider.h              # Rider-specific state (health, weapon, etc.)
│   │   │   └── ai_controller.h      # NPC decision-making data
│   │   └── systems/
│   │       ├── render_system.h/.cpp  # Iterates renderables → submits draw calls
│   │       ├── physics_sync.h/.cpp   # Syncs Jolt transforms ↔ ECS transforms
│   │       ├── rider_system.h/.cpp   # Combat logic (punch, kick, weapon swing)
│   │       ├── ai_system.h/.cpp      # NPC driving & combat AI
│   │       └── hud_system.h/.cpp     # Speedometer, health bar, position overlay
│   │
│   ├── gameplay/
│   │   ├── race_manager.h / .cpp    # Lap tracking, checkpoints, race state
│   │   ├── combat.h / .cpp          # Hit detection, damage, knockoff logic
│   │   └── progression.h / .cpp     # Unlocks, bike upgrades, cash system
│   │
│   ├── audio/                       # (future)
│   │   ├── audio_manager.h / .cpp   # Init, play SFX / music
│   │   └── sound_bank.h             # Named sound handles
│   │
│   └── ui/
│       ├── ui_manager.h / .cpp      # Immediate-mode or retained UI overlay
│       ├── main_menu.h / .cpp       # Title screen, bike select, options
│       └── hud.h / .cpp             # In-race HUD rendering
│
├── assets/
│   ├── models/
│   │   ├── bikes/                   # .glb / .gltf motorcycle meshes
│   │   ├── riders/                  # Rider character meshes + rigs
│   │   ├── environment/             # Road segments, buildings, props
│   │   └── weapons/                 # Bats, chains, clubs
│   ├── textures/                    # Diffuse, normal, roughness maps
│   ├── animations/                  # Rider & bike animation clips (.glb)
│   ├── audio/                       # .ogg / .wav SFX and music
│   ├── tracks/
│   │   └── track_01.json            # Track definition (spline, spawn points, props)
│   └── shaders/                     # Compiled shader binaries (.bin)
│
├── tools/
│   ├── blender/
│   │   ├── export_track.py          # Blender script → track_XX.json
│   │   └── export_model.py          # Blender script → .glb with metadata
│   └── asset_pipeline.bat           # Batch: compile shaders + convert assets
│
├── third_party/                     # Headers / libs not in vcpkg (if any)
│
└── docs/
    ├── game_design.md               # GDD: mechanics, progression, track design
    └── coding_standards.md          # Style guide, naming conventions
```

---

## Module Responsibilities

### `app/` — Application Shell

Owns the **game loop** (`init → update → render → shutdown`). Creates and wires together all subsystems. Manages top-level state transitions (menu → race → results).

### `core/` — Shared Foundations

Zero-dependency utilities used everywhere: math aliases, timing, configuration loading, logging. Nothing in `core/` should depend on bgfx, Jolt, or GLFW directly.

### `input/` — Input Abstraction

Wraps GLFW key/gamepad polling behind an **action map** so the rest of the code never queries raw keycodes. Supports rebinding.

### `renderer/` — Graphics

Initializes bgfx, manages views and frame submission. Owns mesh/material/texture GPU resources. Contains the **shader source** (`.sc` files) and a build script to compile them with `shaderc`.

### `physics/` — Simulation

Wraps Jolt Physics. `PhysicsWorld` steps the simulation at a fixed rate. `VehicleBody` encapsulates motorcycle dynamics (acceleration, lean, wheelies). `ContactListener` dispatches gameplay events on collision.

### `scene/` — Entity-Component-System (ECS)

Lightweight, hand-rolled ECS (or integrate [entt](https://github.com/skypjack/entt) later). Components are plain data structs; systems iterate them each frame. Keeps rendering, physics, and gameplay logic decoupled.

### `gameplay/` — Game Rules

Race management (laps, checkpoints, positions), combat resolution (damage, knockoffs), and progression (cash, upgrades, unlocks). Operates on ECS components — no direct GPU or physics calls.

### `ui/` — User Interface

Renders 2D overlays: main menu, HUD (speedometer, health, minimap). Can use bgfx 2D batching or integrate [Dear ImGui](https://github.com/ocornut/imgui) for debug UI.

### `audio/` — Sound (future)

Placeholder for engine roar, punch impacts, music. Likely candidates: **miniaudio** (single-header) or **FMOD**.

---

## Data Flow — One Frame

```
1.  Input       →  poll GLFW events, update action map
2.  AI          →  NPC decision-making (steering, combat intent)
3.  Gameplay    →  process combat hits, lap logic, race state
4.  Physics     →  Jolt step (fixed dt), resolve contacts
5.  Sync        →  copy Jolt transforms → ECS transforms
6.  Camera      →  update chase-cam from player transform
7.  Render      →  cull, sort, submit draw calls via bgfx
8.  UI / HUD    →  overlay 2D elements
9.  Frame       →  bgfx::frame() — present
```

---

## Build & Asset Pipeline

### C++ Build

```
cmake --preset default        # configure (vcpkg auto-installs deps)
cmake --build build           # compile
./build/RoadPorrada.exe       # run
```

### Shader Compilation

bgfx shaders are written in `.sc` (shader-lang) and compiled with `shaderc`:

```
tools/asset_pipeline.bat      # compiles .sc → .bin for target platform
```

### Blender → Game

1. Model / animate in Blender.
2. Run `tools/blender/export_model.py` to export `.glb` with custom metadata (collision shape type, LOD hints).
3. Track layouts exported via `export_track.py` → `assets/tracks/track_XX.json` (road spline + spawn points + prop placements).

---

## Key Design Decisions

| Decision                                       | Rationale                                                                              |
| ---------------------------------------------- | -------------------------------------------------------------------------------------- |
| **Hand-rolled ECS over a full engine**         | Maximum control over memory layout & frame budget; fits the bgfx philosophy.           |
| **Fixed physics timestep**                     | Jolt requires deterministic stepping; gameplay runs at 60 Hz regardless of render FPS. |
| **Action-mapped input**                        | Decouples gameplay from raw keycodes; makes gamepad + keyboard support trivial.        |
| **Shader-lang (.sc) instead of raw HLSL/GLSL** | bgfx's cross-compiler outputs correct shaders for every backend from one source.       |
| **glTF / GLB for models**                      | Industry standard, excellent Blender support, embeds materials and animations.         |
| **JSON track files**                           | Human-readable, easy to version-control, simple to parse with nlohmann/json.           |

---

## Roadmap Phases

1. **Foundation** — Window + bgfx render loop, load a static mesh, free-fly camera. _(current)_
2. **Motorcycle** — Jolt vehicle body, basic throttle/brake/lean, chase camera.
3. **Track** — Spline-based road generation, environment props, checkpoints.
4. **Combat** — Rider animations, punch/kick hit detection, knockoff physics.
5. **AI Riders** — NPC pathfinding along spline, rubber-banding, combat behavior.
6. **Progression** — Race results, cash, bike shop, upgrades.
7. **Polish** — Audio, particle effects, post-processing, UI menus.
