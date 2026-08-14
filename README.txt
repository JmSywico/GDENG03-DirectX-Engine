GDENG03 DirectX 11 Game Engine

Name:
Juan Morcwel D. Sy-Wico

Submission Contents

Source code:
Game/
DX3D/
DirectXGame.sln
DirectXGame.vcxproj
Scene.level
Scene.dx3dscene
Scene_From_Unreal.level
unity.level

Dependencies:
DX3D/External/ImGui
DX3D/External/ReactPhysics3D

Editor scripts:
Tools/LevelImporters/Unity/ImportDX3DLevelUnity.cs
Tools/LevelImporters/Unreal/import_dx3d_level_unreal.py
Tools/LevelImporters/Unreal/init_unreal.py

How to Run

1. Open DirectXGame.sln in Visual Studio.
2. Set the build platform to x64.
3. Build the solution using Ctrl + Shift + B.
4. Run the program using Ctrl + F5.

Requirements:
Visual Studio with Desktop Development with C++ installed.
Windows SDK installed.
A Windows machine with DirectX 11 support.

Entry Class

The entry class is MainGame.

Entry class files:
Game/MainGame.h
Game/MainGame.cpp

Main Function

The main function is located in:
Game/main.cpp

The program creates a MainGame object and starts it using:
game.run();
