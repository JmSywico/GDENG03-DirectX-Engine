# jnpf. development guide

This guide is for contributors changing the current repository. Read [Project status](PROJECT_STATUS.md) first: production `DX3D` code and canonical `engine` code compile into different targets and require deliberate integration.

## Build graph

| CMake target | Source ownership | Dependencies | Purpose |
|---|---|---|---|
| `jnpf` | `DX3D/Source`, `Game`, production serializer, vendored ImGui | EnTT, Jolt, Windows/DX11 libraries | Runnable editor/application |
| `jnpf_dx11_backend` | canonical DX11 context, mesh, texture, shadow resource code | fmt, Windows/DX11 libraries | Backend compile/test dependency |
| `jnpf_editor_ui` | canonical layers and editor panels | EnTT, JSON, fmt, spdlog, ImGui docking, D3D11 | Compile-check modular workbench |
| `jnpf_scene_tests` | canonical scene/ECS/editor/assets/physics slices and tests | backend, Assimp, EnTT, fmt, spdlog, JSON, Jolt | Registered regression executable |
| `docs` | Doxyfile and maintained docs | Doxygen, optional | Generate HTML API docs |

The default build includes all non-optional targets. `BUILD_TESTING` defaults through CTest and is enabled explicitly by the presets.

## Choosing the correct code path

Change `DX3D/` and `Game/` when the behavior must appear in the runnable application now. Change `engine/` when extending the canonical architecture or its regression contracts. If a feature must exist in both, implement and test each representation consciously; identical class names do not imply ABI or semantic compatibility.

Avoid introducing a new third implementation. Prefer moving production behavior toward the canonical boundary in reviewable slices while keeping `jnpf` buildable.

## Production conventions

### Ownership and lifetime

- `Game` owns top-level services with `UniquePtr`/`RefPtr` aliases.
- `World` owns `GameObject` memory; outside code keeps non-owning pointers.
- Components are owned by the world's EnTT registry.
- Direct3D COM resources use `Microsoft::WRL::ComPtr`.
- Renderer caches must remove resources whose source component is no longer active/alive.

### Structural mutation

Create/destroy operations are deferred through `World` events. Do not erase object storage while iterating updates. Object destruction recursively queues children and invalidates their EnTT entities when flushed.

Use `World::setParent` rather than writing hierarchy pointers. Use Transform setters rather than mutating cached matrices; setters mark the full affected subtree dirty.

### Editor transactions

Authoring changes should:

1. be rejected outside Edit Mode when appropriate;
2. call `pushUndoSnapshot` before mutation;
3. update selection safely if objects are created or destroyed;
4. set `m_sceneDirty`; and
5. provide a useful status/Console message for failures.

For continuous ImGui controls, capture the pre-edit snapshot on `ImGui::IsItemActivated()` so a drag produces one undo step.

### Rendering

Keep C++ and `Basic.hlsl` constant-buffer layouts byte-for-byte aligned. Arrays and scalar groups use 16-byte packing. When adding an SRV or sampler, assign an explicit register and unbind resources that will become render targets in a later pass.

The production renderer is called twice per UI frame. Any per-frame cache or counter must distinguish the Scene and Game pass or intentionally combine them.

### Physics

Keep Jolt types inside `PhysicsWorld.cpp`. Public components and serialized data use engine enums, stable entity IDs, vectors, and scalar properties. Validate finite positive dimensions before creating shapes. Preserve the sync direction:

- static/kinematic: scene to physics;
- dynamic root bodies: physics to scene.

## Adding a production component

The usual path is:

1. Add the header under `DX3D/Include/DX3D/Component` and implementation under `DX3D/Source/DX3D/Component`.
2. Derive from `Component` and declare its runtime type with `dx3d_typeid`.
3. Include it from `DX3D/Include/DX3D/All.h` if it belongs to the public umbrella.
4. Add it to `ComponentCatalog` when users should attach/remove it in Inspector.
5. Add update/render/physics behavior at the owning subsystem boundary.
6. Extend `SerializedSceneObject`, versioned read/write logic, validation, and reconstruction.
7. Increment the production scene payload version when the byte layout changes; keep older-version reads conditional.
8. Extend copy/paste state if the component should survive clipboard operations.
9. Add Inspector editing and undo coverage.
10. Update this handbook and add regression coverage where the canonical test harness can represent the behavior.

The production serializer implementation currently lives at `DX3D/Include/DX3D/Game/SceneSerializer.cpp` and is added explicitly by CMake. Preserve that target membership unless moving the file and build references together.

## Adding geometry

Built-in primitives need:

- component type and mesh generation;
- renderer vertex/index resources and draw selection;
- editor creation menu entry and naming;
- picking mesh access;
- copy/paste and merge handling when applicable;
- scene type/version support; and
- physics collider defaults if relevant.

Imported/combined meshes use `MeshData` with position, color, and normal per vertex plus 32-bit indices. Increment the component mesh revision whenever its data changes so cached GPU buffers rebuild.

## Adding canonical functionality

Canonical mutations should go through `jnpf::Scene::Scene` setters or explicitly call the right dirty/version method after direct component changes. Persistent references use stable `IDComponent` values, not `entt::entity`.

Editor mutations should be commands/snapshots so dirty revisions and undo behavior stay observable. CPU asset preparation may run in jobs, but Direct3D finalization and scene structural commits belong on the engine thread.

When adding a canonical source file, add it to the relevant explicit CMake target. The canonical targets intentionally avoid recursive globs.

## Tests

Run both configurations for changes affecting serialization, undefined behavior, floating-point tolerances, or optimization-sensitive code:

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug

cmake --preset x64-release
cmake --build --preset x64-release
ctest --preset x64-release
```

`Tests/SceneRegressionTests.cpp` uses a small `TestContext` rather than a third-party test framework. Add a focused `Test...` function, call it from `main`, and make expectation messages describe the contract rather than the implementation.

Current coverage includes:

- hierarchy, transforms, stable IDs, deferred destruction, and activation;
- structure/render versions and per-chunk dirtiness;
- bounds, frustum culling, instance-data encoding, and submission planning;
- serialization, malformed input, migrations, prefabs, snapshots, commands, and dirty revisions;
- assets, materials, project metadata, texture failures, and model preparation;
- jobs and parallel transform work;
- cameras, lights, shadows, input, Rotator, Fly Controller, and viewport plans; and
- Jolt bodies, raycasts, contacts, falling cubes, and physics stress.

For production UI/renderer changes, also run the editor and exercise startup, both viewports, scene load/save, Play/Stop restoration, resize, and shutdown. The automated suite does not create the full production window.

## Dependencies

Dependency pins are declared in the root `CMakeLists.txt`. Keep third-party sources in the build tree through `FetchContent`; do not copy generated `_deps` content into `DX3D`, `engine`, or `Game`.

When changing a pin:

1. update the URL/tag/commit;
2. configure Debug and Release from clean preset caches when necessary;
3. build all targets;
4. run both test presets; and
5. update the dependency table in [Architecture](ARCHITECTURE.md).

The production ImGui sources are vendored separately from the canonical docking dependency. An ImGui upgrade must account for both until the UI implementations converge.

## Documentation workflow

Documentation under `docs/` is source-controlled. Generated Doxygen output under `docs/doxygen/` is ignored.

For every user-visible feature:

- update `USER_GUIDE.md` for behavior and controls;
- update `SCENES_AND_ASSETS.md` for persistence/import changes;
- update `ARCHITECTURE.md` for ownership or frame-flow changes;
- update `PROJECT_STATUS.md` when a migration boundary changes; and
- add or improve `/** ... */` API comments on public declarations.

Validate Markdown links and run the Doxygen target when available. See [API documentation](DOXYGEN.md).

## Packaging notes

The active `cmake --install` rules package the production editor executable, `DX3D/Assets`, and `Scenes`. `cmake/PackageRuntime.cmake` describes a stricter canonical project package flow, but it is not called by an active target and expects canonical runtime/shader inputs that are not produced by the current executable build. Do not advertise it as the shipping command until it is integrated and tested end to end.
