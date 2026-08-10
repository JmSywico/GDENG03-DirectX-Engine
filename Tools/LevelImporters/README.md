# DX3D .level Unity/Unreal Tools

Use `File > Export .level` in the DX3D editor to write `Scene.level`.

Unity:

1. Copy `ImportDX3DLevelUnity.cs` into a Unity project's `Assets/Editor` folder.
2. In Unity, choose `Tools > DX3D > Import .level`.
3. Select the exported `Scene.level`.
4. To send edits back to DX3D, choose `Tools > DX3D > Export .level`, then load that `.level` file in the DX3D editor.

Unreal:

1. Copy `import_dx3d_level_unreal.py` into your Unreal project.
2. Put `Scene.level` in the Unreal project root, or call `import_dx3d_level("C:/path/to/Scene.level")` from the Python console.
3. Run `exec(open(r"C:/path/to/import_dx3d_level_unreal.py").read())` from Unreal's Python console or editor scripting tools.
4. Import a level with `import_dx3d_level(r"C:/path/to/Scene.level")`.
5. To send edits back to DX3D, call `export_dx3d_level(r"C:/path/to/Scene.level")` from Unreal's Python console.

Unreal imports assign generated solid-color materials under `/Game/DX3DImported/Materials` so primitives do not use Unreal's default checker material. Unreal exports also write a `material.color` block for primitive actors so DX3D does not fall back to plain white.

10,000-object stress file:

1. Run `python generate_dx3d_10000_level.py C:/path/to/Scene_10000.level`.
2. Load the generated file in DX3D with `File > Import .level`.

The `.level` file stores primitive type, name, transform, material data, optional directional light data, and optional rigid body data. DX3D rotations are written in radians; Unity and Unreal use degrees, so the scripts convert rotations while importing and exporting.

Unity imports create a material for every primitive. If a DX3D texture path points to a file next to the exported `.level` file, the Unity importer copies that image into `Assets/DX3DImported/Textures` and assigns it to the generated material.
