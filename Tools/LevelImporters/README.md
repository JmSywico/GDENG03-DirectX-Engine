# DX3D .level Unity/Unreal Tools

Use `File > Export .level` in the DX3D editor to write `Scene.level`.

Directory layout:

* `Unity/ImportDX3DLevelUnity.cs`
* `Unreal/import_dx3d_level_unreal.py`
* `Unreal/init_unreal.py`

Unity:

1. Copy `Unity/ImportDX3DLevelUnity.cs` into a Unity project's `Assets/Editor` folder.
2. In Unity, choose `Tools > DX3D > Import .level`.
3. Select the exported `Scene.level`.
4. The importer creates a fresh empty Unity scene, then loads the `.level` objects into that scene.
5. To send edits back to DX3D, choose `Tools > DX3D > Export .level`, then load that `.level` file in the DX3D editor.

Unreal:

1. Copy `Unreal/import_dx3d_level_unreal.py` and `Unreal/init_unreal.py` into your Unreal project's `Content/Python` folder. If your project already has an `init_unreal.py`, copy the two lines from this file into the existing one instead.
2. Restart Unreal, or run `import import_dx3d_level_unreal; import_dx3d_level_unreal.install_unreal_editor_menu()` once from the Python console.
3. Use the Tools menu entries `Import DX3D .level...` and `Export DX3D .level...`.
4. If your Unreal version cannot open a file dialog, put `Scene.level` beside your Unreal `.uproject` file. The menu import/export will use that project-folder file automatically.
5. The importer creates a fresh Unreal level under `/Game/DX3DImported/Levels`, then loads the `.level` actors into that level.
6. Console fallback: `import_dx3d_level(r"C:/path/to/Scene.level")` or `export_dx3d_level(r"C:/path/to/Scene.level")`.

Unreal imports assign generated solid-color materials under `/Game/DX3DImported/Materials` so primitives do not use Unreal's default checker material. Unreal exports also write a `material.color` block for primitive actors so DX3D does not fall back to plain white.

Unreal Content Browser drag-and-drop for `.level` files would require a custom Unreal editor importer plugin/factory. These Python tools provide menu-and-dialog import/export without needing a C++ plugin.

The `.level` file stores primitive type, name, transform, material data, optional directional light data, and optional rigid body data. DX3D rotations are written in radians; Unity and Unreal use degrees, so the scripts convert rotations while importing and exporting.

Unity imports create a material for every primitive. If a DX3D texture path points to a file next to the exported `.level` file, the Unity importer copies that image into `Assets/DX3DImported/Textures` and assigns it to the generated material.
