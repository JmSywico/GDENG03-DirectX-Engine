# jnpf.

jnpf. is a Windows-only C++20 game-engine and editor project built on native DirectX 11. The runnable application combines a borderless ImGui editor, an EnTT-backed object/component world, versioned scene persistence, Jolt physics, primitive and OBJ rendering, lighting, shadows, editor cameras, transform gizmos, and isolated Play Mode.

The repository is also the workspace for a newer canonical engine layer under `engine/`. Those libraries compile and are regression-tested independently while integration with the runnable `DX3D` application continues. See [Project status](docs/PROJECT_STATUS.md) before choosing an API surface.

## What works today

- Native DirectX 11 rendering with scene and game viewports
- Cube, sphere, cylinder, capsule, plane, circle, and imported OBJ geometry
- Directional, point, and spot lights; up to 16 active lights
- Directional, spot, and point-light shadows
- Materials, emissive color, texture sampling, and debug material modes
- Object hierarchy, stable IDs, activation state, selection, multi-selection, and reparenting
- Inspector editing, transform gizmos, undo/redo, copy/paste, duplicate, and merge
- Jolt static, dynamic, and kinematic bodies with box, sphere, cylinder, and capsule colliders
- Fixed-step Play Mode with pause, single-step, and restoration of the editor scene on Stop
- Versioned `.dx3dscene` files and compatibility loading for older binary scenes
- Asset browser, OBJ import, texture drag-and-drop, stats, and console panels
- Debug and Release CMake presets plus a regression suite

## Requirements

- 64-bit Windows 10 or 11
- Visual Studio 2022 or newer with **Desktop development with C++**
- MSVC, a Windows 10/11 SDK, CMake 3.22+, and Ninja
- Internet access during the first CMake configure so pinned dependencies can be downloaded
- A DirectX 11-capable GPU and driver

Only an x64 MSVC build is supported. CMake rejects Win32, non-MSVC, and non-Windows toolchains.

## Quick start

Run these commands from an **x64 Native Tools Command Prompt for Visual Studio** or an equivalent terminal with MSVC initialized:

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug
./out/build/x64-debug/bin/jnpf.exe
```

To open a converted sample scene at startup:

```powershell
./out/build/x64-debug/bin/jnpf.exe Scenes/jnpf/falling-cubes.dx3dscene
```

The executable accepts one optional positional argument: a `.dx3dscene` path. Run it with the repository root as the working directory, or run from its output directory where CMake copies the required shader and scene assets.

The checked-in Visual Studio solution is a convenience path after the matching CMake preset has populated dependency headers and Jolt libraries under `out/build`. CMake presets remain authoritative; use the x64 configurations only.

## Documentation

- [Getting started](docs/GETTING_STARTED.md) — installation, build, run, test, install, and common failures
- [Editor and runtime guide](docs/USER_GUIDE.md) — panels, controls, authoring, Play Mode, and physics workflow
- [Architecture](docs/ARCHITECTURE.md) — frame flow, object model, renderer, physics, persistence, and repository boundaries
- [Scenes and assets](docs/SCENES_AND_ASSETS.md) — formats, compatibility, sample scenes, OBJ import, and textures
- [Development guide](docs/DEVELOPMENT.md) — targets, code conventions, adding features, tests, and API docs
- [Current project status](docs/PROJECT_STATUS.md) — implemented surfaces, migration boundary, and known limitations
- [API documentation](docs/DOXYGEN.md) — generating and maintaining Doxygen output

## Repository map

| Path | Purpose |
|---|---|
| `DX3D/` | Production implementation used by the runnable editor executable |
| `Game/` | Application entry point and editor-camera behavior |
| `engine/` | Canonical engine libraries being integrated incrementally |
| `Tests/` | Canonical scene, editor, rendering-planning, asset, and physics regressions |
| `Scenes/` | Source scenes and converted `.dx3dscene` files |
| `assets/` | Project models and textures shown by Asset Lens |
| `config/` | Canonical input-action configuration |
| `cmake/` | Supporting CMake scripts |
| `docs/` | Maintained project handbook and generated API-doc output |

## Primary targets

| Target | Role |
|---|---|
| `jnpf` | Runnable DirectX 11 editor/application |
| `jnpf_dx11_backend` | Canonical DirectX 11 resource/backend library |
| `jnpf_editor_ui` | Compile-checked canonical editor workbench library |
| `jnpf_scene_tests` | Regression test executable registered with CTest |
| `docs` | Doxygen target, available when Doxygen is installed at configure time |

## Authors

- Naomi Jade V. Inoferio
- Paul Davidion D. Macaraeg
- Francis Gabriel M. Obina
- Juan Morcwel D. Sy-Wico

## License

No project license file is currently included. Do not assume redistribution or reuse rights beyond applicable law; contact the authors for permission. Third-party dependencies retain their own licenses.
