# Current jnpf. project status

This page distinguishes production behavior from code that only compiles or runs in regression tests. It is the practical source of truth for the repository's current integration state.

## Status vocabulary

- **Production**: reachable in the `jnpf` executable.
- **Compile-checked**: built by CMake but not connected to the executable.
- **Regression-tested**: exercised by `jnpf_scene_tests` without the full production editor.
- **Pending**: represented by code or design intent but not complete end to end.

## Target status

| Target | Status | Notes |
|---|---|---|
| `jnpf` | Production | Native DX11 editor/application built from `DX3D` and `Game` |
| `jnpf_dx11_backend` | Compile-checked and test dependency | Canonical context, targets, mesh, texture, and shadow resources |
| `jnpf_editor_ui` | Compile-checked | Canonical modular workbench; not linked into `jnpf` |
| `jnpf_scene_tests` | Regression-tested | Canonical scene/editor/assets/render planning/physics suite |
| Editor-free runtime | Pending | No active standalone runtime target |
| Canonical folder package | Pending | Baseline install exists; strict project package script is not integrated |

## Feature matrix

| Area | Production application | Canonical layer |
|---|---|---|
| Platform | Borderless Win32 window, DPI scaling, custom title bar | Window and title-bar modules staged/compile-checked |
| Graphics | Native D3D11 device, swap chain, two captured viewports, HLSL pipeline | Context/RTV/DSV/SRV, mesh, WIC texture, and shadow resources compile |
| Scene model | `GameObject` ownership plus EnTT components and stable IDs | Data-oriented `Scene`, stable IDs, versions, dirty roots, deferred destroy tested |
| Hierarchy | Parenting, activation, transform propagation, drag/drop | Cycle checks, keep-world parenting, commands, propagation tested |
| Geometry | Cube, sphere, cylinder, capsule, plane, circle, combined/OBJ | Procedural primitives and model preparation tested |
| Renderer | Per-object submissions, 16 lights, material/texture/shadow support | Chunk bounds/culling and instance-submission planning tested; integration pending |
| Editor | Scene/Game, Elements, Inspector, gizmos, undo/redo, assets, stats, console | Modular layers/panels/context compile independently |
| Persistence | `.dx3dscene` envelope plus old binary compatibility, payload v12 | Structured `.escene`, migrations, snapshots, prefabs tested |
| Play Mode | Isolated snapshot, 60 Hz physics, pause/step/restore | Simulation/editor contracts represented and tested in slices |
| Physics | Jolt bodies, four shapes, contacts, raycast, statistics | Jolt wrapper and stress regressions pass |
| Assets | Asset Lens, built-in OBJ import, WIC texture loading | Registry/meta IDs, Assimp preparation, materials, prefabs tested |
| Input | Raw key/mouse state, cursor ownership, compiled action defaults | Configurable input actions tested |
| Logging | Production in-memory/std stream Console | spdlog-backed canonical logging compiles/tests |

## Important current boundaries

### Two scene APIs

`dx3d::World`/`GameObject` and `jnpf::Scene::Scene` are separate models. A feature implemented only under `engine/` does not automatically appear in the editor. Scene files also require explicit conversion between `.escene` and `.dx3dscene`.

### Two editor implementations

The visible editor is implemented inside production `dx3d::Game`. Canonical panels under `engine/Editor` compile as a static library to keep their API and design healthy, but no canonical engine host currently drives them.

### Two rendering levels

The production renderer is functional and feature-rich but submits objects directly. Canonical chunk caches, visibility planning, and instance submission are regression-tested data paths; they are not yet the production draw path.

### Configuration is not yet unified

`jnpf.jnpfproject` and `config/input-actions.json` belong to the canonical project/runtime design. The production entry point uses a positional `.dx3dscene` argument and `InputActionMap::createDefaults()` instead.

## Known limitations

- Windows x64 MSVC is the only supported platform/toolchain.
- There is no active headless build, standalone game runtime, or cross-platform renderer.
- Production scene saves are identifiable JSON but their hexadecimal payload is not meant for hand editing.
- New Scene exists internally but is disabled in the current File menu.
- There is no active Save As dialog; Save writes the current path.
- Production import is OBJ-only and ignores UV/material-library data.
- Asset Lens labels more formats than the production importer can open.
- The first active shadow-casting light owns the shadow pass; multiple simultaneous shadow maps are not submitted.
- Production directional and spot shadows use one projected map; point shadows use six cube faces.
- Dynamic rigid bodies must be hierarchy roots for physics-to-scene synchronization.
- Production rendering does not yet use canonical instancing, so extreme instance scenes are expensive.
- Automated tests cover canonical subsystems heavily but do not automate the full production window/editor interaction.
- The Visual Studio project depends on CMake-populated dependency folders and is secondary to presets.

## Recommended next integration milestones

1. Choose the canonical `Scene` as the production world boundary or build a temporary explicit adapter.
2. Host `jnpf_editor_ui` from a canonical executable using the existing DX11 SRV providers.
3. Connect canonical chunk visibility and bounded instance submission to the DX11 draw backend.
4. Unify `.escene` authoring, project discovery, and configurable input in the production host.
5. Add an editor-free runtime target and make the strict folder package script an active verified target.
6. Add window-level smoke/integration tests for load, Play/Stop, resize, and shutdown.

These are sequencing recommendations, not claims that the current editor is unusable. The production `DX3D` application remains the working baseline while integration proceeds.
