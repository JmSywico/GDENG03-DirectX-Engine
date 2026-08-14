GDENG03 DirectX 11 Game Engine

Name:
Juan Morcwel D. Sy-Wico

Submission Contents

Source code:

* Game/
* DX3D/
* DirectXGame.sln
* DirectXGame.vcxproj
* Scene.level
* Scene.dx3dscene
* Scene_From_Unreal.level
* unity.level

Dependencies:

* DX3D/External/ImGui
* DX3D/External/ReactPhysics3D

Editor scripts:

* Tools/LevelImporters/Unity/ImportDX3DLevelUnity.cs
* Tools/LevelImporters/Unreal/import_dx3d_level_unreal.py
* Tools/LevelImporters/Unreal/init_unreal.py

How to Run

1. Open the .sln file in Visual Studio.
2. Set the build platform to x64.
3. Build the solution using Ctrl + Shift + B.
4. Run the program using Ctrl + F5.

Make sure Visual Studio has Desktop Development with C++, the Windows SDK, and a Windows machine with DirectX 11 support.

Entry Class

The entry class is MainGame.

Files:

* MainGame.h
* MainGame.cpp

MainGame is responsible for creating the scene and updating the program every frame.

Main Function

The main function is located in:

Game/main.cpp

The program creates a MainGame object and starts it using:

game.run();
