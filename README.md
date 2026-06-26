GDENG03 DirectX 11 Game Engine

Name:
Juan Morcwel D. Sy-Wico

How to Run

1. Open the .sln file in Visual Studio.
2. Set the build platform to x64.
3. Build the solution using Ctrl + Shift + B.
4. Run the program using Ctrl + F5.

Make sure Visual Studio has Desktop Development with C++ and the Windows SDK installed.

Entry Class

The entry class is MainGame.

Files:

* MainGame.h
* MainGame.cpp

MainGame is responsible for creating the scene and updating the program every frame.

Main Function

The main function is located in:

main.cpp

The program creates a MainGame object and starts it using:

game.run();
