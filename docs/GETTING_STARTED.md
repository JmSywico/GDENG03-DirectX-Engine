# Getting started

This guide covers the supported CMake workflow for configuring, building, testing, running, and installing enignE.

## Supported environment

enignE deliberately targets a narrow platform:

- Windows 10 or Windows 11, 64-bit
- MSVC with C++20 support
- Windows SDK with Direct3D 11, DXGI, D3DCompiler, Windows Imaging Component, and Win32 headers/libraries
- CMake 3.22 or newer
- Ninja
- A DirectX 11-capable graphics adapter

Install the **Desktop development with C++** workload in Visual Studio Installer. Include the MSVC x64 build tools, a Windows SDK, CMake tools, and Ninja. The first configure requires network access because CMake downloads pinned versions of EnTT, fmt, spdlog, nlohmann/json, Assimp, Dear ImGui docking, and Jolt Physics into the ignored build tree.

## Configure

Open an **x64 Native Tools Command Prompt for Visual Studio**. PowerShell is fine if the Visual Studio developer environment has already been initialized.

From the repository root:

```powershell
cmake --preset x64-debug
```

The preset uses Ninja and writes to `out/build/x64-debug`. To prepare an optimized build:

```powershell
cmake --preset x64-release
```

CMake fails early when the environment is not Windows, MSVC, or x64. That failure is intentional; Win32 configurations displayed by the checked-in Visual Studio solution are not supported.

## Build

```powershell
cmake --build --preset x64-debug
```

Useful target-specific builds:

```powershell
cmake --build --preset x64-debug --target enignE
cmake --build --preset x64-debug --target enignE_scene_tests
cmake --build --preset x64-debug --target enignE_editor_ui
```

The runnable executable is written to:

```text
out/build/x64-debug/bin/enignE.exe
```

After linking `enignE`, CMake copies `DX3D/Assets` and `Scenes` beside the executable. Rebuilding the target refreshes those copied resources.

## Run

From the repository root:

```powershell
./out/build/x64-debug/bin/enignE.exe
```

The default scene path is `Scene.dx3dscene`. You can pass a different converted scene as the only positional argument:

```powershell
./out/build/x64-debug/bin/enignE.exe Scenes/enignE/rotator-demo.dx3dscene
```

Paths may be absolute or relative to the process working directory. The editor also expects the HLSL shader at `DX3D/Assets/Shaders/Basic.hlsl`, so either:

- launch from the repository root; or
- change into `out/build/x64-debug/bin`, where the post-build copy created the same relative asset layout.

The application returns failure if initialization throws. Runtime errors are also sent to the in-editor Console and the standard logging stream.

## Test

Run the registered suite after a build:

```powershell
ctest --preset x64-debug
```

For optimized-code coverage:

```powershell
cmake --preset x64-release
cmake --build --preset x64-release
ctest --preset x64-release
```

The current CTest suite is `scene_regressions`. It exercises scene hierarchy and transforms, stable IDs, serialization and migration, undo/redo commands, assets and materials, chunk invalidation and culling, instance submission planning, viewport planning, jobs, input behavior, shadows, and Jolt physics including sample stress scenes.

## Visual Studio solution

`DirectXGame.sln` builds the production `DX3D` application directly. Its x64 project settings refer to dependency headers and Jolt libraries beneath the CMake preset directories. Configure and build the corresponding CMake preset at least once before using the solution:

1. Run `cmake --preset x64-debug` and `cmake --build --preset x64-debug`.
2. Open `DirectXGame.sln`.
3. Select `Debug | x64` or `Release | x64`.
4. Build and run `DirectXGame`.

Do not select a Win32 solution configuration. Prefer CMake whenever target membership or dependency configuration differs between the two build descriptions.

## Install a runnable folder

The baseline install rule stages the editor executable, DirectX shader assets, and scenes:

```powershell
cmake --preset x64-release
cmake --build --preset x64-release
cmake --install out/build/x64-release --prefix dist/enignE
```

The installed program remains the editor-enabled `enignE` executable. A separate editor-free runtime package is not yet an active CMake target.

## Generate API documentation

Install Doxygen, reconfigure so CMake can find it, then run:

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug --target docs
```

Open `docs/doxygen/html/index.html`. Graphviz is optional and disabled by default. See [API documentation](DOXYGEN.md).

## Troubleshooting

### CMake reports that the toolchain is unsupported

Confirm the terminal is an x64 Visual Studio developer shell and that `cl.exe` resolves to MSVC. The project does not support MinGW, Clang-only configurations, Win32, macOS, or Linux.

### Dependency download fails

The first configure downloads pinned archives. Check network, proxy, certificate, and GitHub access, then rerun the same configure command. Successful downloads remain under `out/build/<preset>/_deps`.

### The shader cannot be opened or compiled

Run from the repository root or the executable output directory. Confirm this file exists relative to that working directory:

```text
DX3D/Assets/Shaders/Basic.hlsl
```

Rebuild `enignE` to refresh the post-build asset copy.

### A scene fails to load

Use a `.dx3dscene` produced for the DX11 application. Selecting a source `.escene` in the dialog only works when a same-named converted `.dx3dscene` exists beside it or under `Scenes/enignE`. See [Scenes and assets](SCENES_AND_ASSETS.md).

### The CMake cache references an old compiler or path

Close processes using the build output, remove only the affected preset directory under `out/build`, and configure that preset again. Do not remove source directories or the repository root.

### The editor opens but the Game view is empty

Ensure the scene has an active, enabled camera other than `Editor Camera`. The editor creates `Main Camera` when none exists, and the first authored camera becomes primary when necessary.
