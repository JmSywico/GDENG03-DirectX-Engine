# Architecture

enignE currently has a production application and a canonical engine migration layer in the same repository. Both use native DirectX 11, but they are not yet one runtime and their scene/component types are not interchangeable.

## Repository boundary

```text
Game/main.cpp
    -> MainGame
        -> dx3d::Game
            -> DX3D world, editor, renderer, input, physics
            -> enignE.exe

engine/Include + engine/Source
    -> canonical scene/editor/assets/physics/DX11 libraries
    -> enignE_dx11_backend
    -> enignE_editor_ui
    -> enignE_scene_tests
```

### Production application

`enignE.exe` is built from `DX3D/Source`, `Game`, the serializer implementation currently stored under `DX3D/Include`, and the vendored ImGui implementation in `DX3D/External/ImGui`. This is the editor users run today.

The entry point constructs `MainGame`, which derives from `dx3d::Game`. `dx3d::Game` owns the top-level services:

- `Logger`
- `InputSystem`
- `GraphicsDevice`
- `Display` and Win32 window
- `World`
- `WorldRenderer`
- `PhysicsWorld`
- ImGui context and editor state

`MainGame` currently contributes Scene-camera navigation and leaves scene authoring, rendering, simulation controls, and panels to `dx3d::Game`.

### Canonical engine layer

The `engine/` tree is a newer modular design under active integration. CMake keeps three slices healthy:

- `enignE_dx11_backend`: device/swap-chain context, render/depth targets, mesh buffers, WIC textures, and shadow targets;
- `enignE_editor_ui`: layers, editor context, title bar, hierarchy, inspector, asset browser, and gizmo toolbar; and
- `enignE_scene_tests`: scene/ECS, command stack, persistence/migrations, assets, jobs, chunk planning, input systems, and Jolt physics.

The canonical tree does not currently provide the executable entry point. Treat it as tested integration work, not as a second runnable editor.

## Production frame flow

Each call to `Game::onInternalUpdate` performs this sequence:

1. Compute variable frame delta.
2. Update keyboard, mouse, cursor-lock, and Win32 window state.
3. Begin a new ImGui frame and process global Play/Pause/Step shortcuts.
4. Call `MainGame::onUpdate` for Scene-camera navigation.
5. Update captured Fly Controllers.
6. Advance the world:
   - Edit Mode: one variable-delta object update, no physics step;
   - Play Mode: 60 Hz fixed updates and physics, up to eight catch-up steps;
   - Paused: no simulation unless one step was requested.
7. Render using the editor camera and copy the result into the Scene viewport texture.
8. Render using the primary authored camera and copy the result into the Game viewport texture.
9. Build editor chrome, dockspace, hierarchy, inspector, diagnostics, asset, and console UI.
10. Render ImGui to the swap-chain back buffer and present.

The process is single-runtime-thread oriented. The production renderer uses the Direct3D immediate context. Jolt is configured with one worker thread in the production `PhysicsWorld`; the canonical job system has separate parallel regression coverage.

## Object and component model

### Production `dx3d::World`

The production model combines object-oriented ownership with EnTT component storage:

- `World` owns `GameObject` instances through `unique_ptr` containers.
- Every object receives a monotonic 64-bit stable ID and an EnTT entity.
- Every object automatically receives a `TransformComponent`.
- Component templates add/query/remove typed values in the world registry.
- An ID index provides stable lookup independently of EnTT's transient entity value.
- Creation and destruction are queued so object containers are not structurally mutated during an update traversal.
- Destroying an object recursively queues its descendants first.
- Parent/child links are explicit pointers; cycle checks guard reparenting.
- Transform dirtiness propagates through the subtree and world matrices update after object updates.

`isActiveSelf` stores local activation. `isActiveInHierarchy` combines it with all ancestor states and is used by update, rendering, physics, and editor markers.

Production components include:

| Category | Components |
|---|---|
| Core | Transform |
| Geometry | Cube, Sphere, Cylinder, Capsule, Plane, Circle, Combined Mesh |
| Rendering | Camera, Material, Texture, Directional/Point/Spot Light |
| Simulation | Rotator, Fly Controller |
| Physics | Rigid Body, Collider |

### Canonical `enignE::Scene::Scene`

The canonical scene owns its EnTT registry directly and uses data-oriented entities with `IDComponent`, hierarchy, transform, renderer, camera, light, behavior, and physics components. It adds:

- structure, transform, and render version counters;
- dirty transform root sets;
- command/snapshot-based editor mutation;
- versioned JSON serialization and migration;
- prefab and asset metadata flows; and
- chunk/cache invalidation contracts.

Code should not pass `dx3d::GameObject`, production components, or production scene bytes to canonical APIs. Integration requires explicit translation until the production executable changes ownership.

## Coordinate and unit conventions

The production renderer uses left-handed view/projection matrices. With an identity transform, `+X` is right, `+Y` is up, and `+Z` is forward. Matrices are stored and uploaded as row-major values, matching `Basic.hlsl` declarations.

Positions and scales use engine units; physics treats one unit as a practical meter-scale unit. Transform Euler rotations and camera field of view are radians internally. Light spot angles and Fly Controller pitch limits are authored in degrees and converted at their subsystem boundaries.

## Rendering

### Device and presentation

`GraphicsDevice` creates a hardware Direct3D 11 device and immediate context. Debug builds request the D3D11 debug device. `Display` owns the Win32 window and swap chain. `SwapChain` owns the back-buffer RTV/DSV plus shader-resource copies used to display Scene and Game inside ImGui.

### World renderer

`WorldRenderer` owns or caches:

- HLSL pipeline state and constant buffer;
- vertex/index buffers for built-in primitives;
- revision-aware buffers for combined/imported meshes;
- WIC-decoded texture, SRV, and sampler resources;
- a projected shadow map and a point-light cube shadow map; and
- failed texture paths to avoid repeated failing loads.

For each render pass it selects either the editor camera or primary authored camera, gathers up to 16 active lights, chooses the first shadow-casting light, renders the relevant shadow pass, fills a shared constant-buffer layout, binds material/texture state, and draws active visible objects.

The shader supports:

- directional, point, and spot attenuation;
- one shadow-driving light per frame;
- comparison-sampled projected shadows;
- cube-map point shadows;
- albedo texture sampling;
- albedo tint and emissive output; and
- four flat/debug material alternatives to the lit mode.

The current production path issues object-oriented draw submissions. Canonical chunk culling and instance submission are tested as planning/data structures but are not wired into `WorldRenderer` yet.

## Physics

Jolt is private behind `dx3d::PhysicsWorld`; scene files contain engine component values, never Jolt handles.

At reset, active objects with both Rigid Body and Collider components become Jolt bodies. Shapes are built from object scale and collider settings. Static and kinematic transforms flow from the scene into Jolt before a step. Dynamic body position and rotation flow from Jolt back into root object transforms after the step.

The production physics API exposes:

- static, dynamic, and kinematic motion;
- box, sphere, cylinder, and capsule shapes;
- friction, restitution, damping, and gravity scale;
- raycasts returning stable entity IDs;
- begin/persist/end contact events;
- active contact snapshots; and
- step time, body, active-body, contact, and event statistics.

Play Mode calls physics at `1/60` second. Edit Mode does not advance the physics simulation.

## Persistence and editor transactions

Production persistence has one versioned binary representation used by:

- scene-file payloads;
- undo and redo snapshots; and
- Play Mode cloning/restoration.

This shared representation prevents the editor transaction paths from drifting apart. Current disk saves wrap the bytes in a small readable JSON envelope. Loading accepts both the envelope and historical raw binary files. See [Scenes and assets](SCENES_AND_ASSETS.md).

Undo and redo store up to 64 scene snapshots. Authoring operations push a snapshot before mutation. Entering Play Mode takes a dedicated editor snapshot, reconstructs the runtime world, then discards runtime changes by deserializing that snapshot on Stop.

## Input ownership

`InputSystem` tracks current/previous key states, raw mouse delta, cursor visibility, locking, and the current lock rectangle. The Scene viewport only consumes navigation when hovered/focused. UI text entry suppresses editor shortcuts. Play Mode can capture input to the Game viewport; `Escape` releases it.

`InputActionMap` provides named scalar actions and edge detection. `MainGame` uses the compiled default map for editor movement. The JSON configuration in `config/` belongs to the canonical project/runtime path and is not loaded by the production executable today.

## Dependencies

| Dependency | Pinned version/source | Current role |
|---|---|---|
| EnTT | 3.14.0 | Production component registry and canonical scene registry |
| Jolt Physics | 5.5.0 | Production and canonical physics implementations |
| Dear ImGui | vendored production copy plus pinned docking commit | Editor UI and compile-checked canonical UI |
| Assimp | 6.0.4 | Canonical model preparation/tests |
| nlohmann/json | 3.12.0 | Canonical scenes, project files, assets, and input configuration |
| fmt | 12.1.0 | Canonical formatting/backend diagnostics |
| spdlog | 1.17.0 | Canonical logging |
| Windows SDK | system | D3D11, DXGI, D3DCompiler, WIC, Win32, common dialogs |

Dependencies fetched by CMake live only beneath `out/build/<preset>/_deps`.
