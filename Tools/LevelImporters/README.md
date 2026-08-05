# DX3D .level Importers

Use `File > Export .level` in the DX3D editor to write `Scene.level`.

Unity:

1. Copy `ImportDX3DLevelUnity.cs` into a Unity project's `Assets/Editor` folder.
2. In Unity, choose `Tools > DX3D > Import .level`.
3. Select the exported `Scene.level`.

Unreal:

1. Copy `import_dx3d_level_unreal.py` into your Unreal project.
2. Put `Scene.level` in the Unreal project root, or call `import_dx3d_level("C:/path/to/Scene.level")` from the Python console.
3. Run the script from Unreal's Python console or editor scripting tools.

The `.level` file stores primitive type, name, transform, optional directional light data, and optional rigid body data. DX3D rotations are written in radians; the importers convert them to degrees.
