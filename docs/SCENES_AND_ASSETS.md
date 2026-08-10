# Scenes and assets

The repository contains two scene families because the production DX11 editor and the canonical migration layer currently use different serializers.

## Scene formats at a glance

| Extension/location | Owner | Use today |
|---|---|---|
| `.dx3dscene` | Production `dx3d::SceneSerializer` | Load/save in the runnable editor and pass on the executable command line |
| `.escene` | Canonical `enignE::Scene::SceneSerializer` | Canonical tests, migration inputs, and source counterparts for converted samples |
| `Scene.dx3dscene` | Production serializer | Default scene when no startup argument is supplied |

The two formats are not interchangeable. The editor's open dialog displays both, but selecting an `.escene` does not parse it directly. Instead, the editor searches for a same-named `.dx3dscene`:

1. beside the selected `.escene`; then
2. under `Scenes/enignE/`.

If neither converted counterpart exists, loading stops with an explanatory Console error.

## Production `.dx3dscene`

### Current disk representation

New saves use a JSON envelope:

```json
{
  "format": "jnpf.scene",
  "version": 1,
  "encoding": "hex",
  "payload": "...versioned scene bytes..."
}
```

The payload is hexadecimal rather than a human-editable component document. Do not manually edit it. Its internal representation is also used for undo/redo and Play Mode snapshots.

The loader first checks for the historical eight-byte `DX3DSCNE` magic. When present, it reads the file as a raw binary scene. Otherwise it validates and unwraps the JSON envelope. This preserves compatibility with existing checked-in binary scenes such as the root `Scene.dx3dscene` while making new files identifiable by ordinary JSON tooling.

### Version and safety limits

The production payload version is currently 12 and accepts versions 1 through 12. Loading rejects unsupported versions and unreasonable allocations before constructing objects.

| Limit | Value |
|---|---:|
| Objects | 100,000 |
| String length | 1 MiB |
| Combined-mesh vertices | 10,000,000 |
| Combined-mesh indices | 30,000,000 |

Scene loading reconstructs into staged data before applying objects, validates enum/component values, repairs camera availability in the editor, restores parent links by stable ID, and reports failure without treating malformed bytes as trusted sizes.

### Persisted production data

Per object, the versioned serializer can retain:

- stable entity ID and parent entity ID;
- object type, name, and local active state;
- local position, quaternion/Euler rotation compatibility data, and scale;
- combined-mesh vertices and indices;
- directional/point/spot light settings;
- material mode, albedo, emissive color, and emission strength;
- texture path and enabled state;
- rigid-body type and material/damping/gravity settings;
- collider shape and dimensions;
- Rotator settings;
- Fly Controller settings; and
- camera clip planes, field of view, aspect ratio, and primary flag.

The editor camera is treated as authoring infrastructure. The editor ensures it exists after create/load/undo/redo/play restoration.

## Canonical `.escene`

Canonical scenes are normal structured JSON used by the `engine/` scene and migration tests. They contain a scene header and an entity array with named component objects. The canonical serializer supports schema migration and stable asset/project identities.

These files are valuable as readable source scenes and test fixtures, but the runnable `DX3D` editor currently needs their converted `.dx3dscene` counterparts. Files named `*.import.txt` record the source scene, entity count, imported count, and warning count used when a conversion was produced.

## Included scenes

| Scene | Purpose |
|---|---|
| `Scene.dx3dscene` | Default production editor scene |
| `Scenes/enignE/editor-test.dx3dscene` | Compact editor feature check |
| `Scenes/enignE/falling-cubes.dx3dscene` | Dynamic-body and collision demonstration |
| `Scenes/enignE/rotator-demo.dx3dscene` | Rotator behavior demonstration |
| `Scenes/enignE/test.dx3dscene` | Small runtime-style sample |
| `Scenes/enignE/physics-stress.dx3dscene` | Large physics workload |
| `Scenes/enignE/instance-stress.dx3dscene` | Very large geometry/instance source workload |

The corresponding `.escene` files are canonical fixtures. The large stress scenes are intended for validation and profiling, not as default authoring templates. The production renderer does not yet use the canonical instance-submission path, so the instance stress scene can be expensive in the runnable editor.

## Saving workflow

`Ctrl+S` writes to the current production scene path. The initial path is `Scene.dx3dscene`; loading another `.dx3dscene` makes that file the current path. There is no active Save As dialog, so use the desired startup file or duplicate/rename a scene externally before editing when you need a separate file.

Save is only performed in Edit Mode. Invoking it while playing stops and restores the editor scene first.

## Startup scene

The executable reads an optional positional scene path:

```powershell
./out/build/x64-debug/bin/enignE.exe Scenes/enignE/falling-cubes.dx3dscene
```

Without an argument, the editor does not read `enignE.enigneproject`; it uses `Scene.dx3dscene`. The project configuration and its `startupScene` field are part of the canonical runtime/package path and are exercised by tests, but not connected to the current production entry point.

## Asset roots

Asset Lens scans these paths relative to the process working directory:

- `assets/`
- `DX3D/Assets/`

It recursively lists regular files, excluding `.meta`, `.escene`, `.dx3dscene`, `.json`, and `.DS_Store`. Its type label recognizes common model, image, material, prefab, and HLSL extensions. Recognition in the UI does not imply that the production runtime imports every format.

## OBJ import

The production editor imports `.obj` files through a compact built-in loader:

- Double-click an OBJ in Asset Lens, or drag it into Scene.
- Import is allowed only in Edit Mode.
- The object is created at the Scene spawn point, five units in front of the editor camera.
- Polygon faces are triangulated as a fan.
- Positive and negative position/normal indices are accepted.
- Supplied normals are used; missing normals are generated per triangle.
- Imported vertices receive white vertex color.
- Texture coordinates and material-library assignments are not imported by this production loader.
- The resulting geometry is stored in a `CombinedMeshComponent` and therefore travels with `.dx3dscene` snapshots/files.

The canonical layer has Assimp-backed CPU model preparation for a broader set of file types. It is tested but not yet connected to production Asset Lens import.

## Textures

Add a Texture component to a renderable object and either enter a path or drag an image from Asset Lens. The production renderer uses Windows Imaging Component to convert the first frame to 32-bit RGBA and creates a Direct3D 11 texture, SRV, and sampler.

The Inspector drag target accepts `.png`, `.jpg`, `.jpeg`, `.bmp`, `.dds`, and `.tga`. Actual decoding depends on WIC codecs available on the machine; PNG, JPEG, and BMP are the most portable baseline. Texture paths are stored as authored strings, so prefer project-relative forward paths such as:

```text
assets/textures/example.png
```

Missing or undecodable textures are logged once and cached as failed paths for the session. Use Asset Lens Refresh after changing files, and restart when retrying a path previously cached as failed.

## Shader assets

The production renderer compiles `DX3D/Assets/Shaders/Basic.hlsl` at startup through D3DCompiler. The same file contains vertex and pixel shader entry points and supports lighting, material modes, texture sampling, and shadow sampling.

CMake copies the entire `DX3D/Assets` directory beside the executable after a successful `enignE` build and includes it in baseline installs. If shader edits appear stale, rebuild the target or run from the repository root.
