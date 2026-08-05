#pragma once

#include <DX3D/Core/Core.h>

#include <string>

namespace dx3d
{
	class World;

	struct SceneLoadResult
	{
		bool success{};
		ui32 cubeCount{};
		ui32 planeCount{};
		ui32 sphereCount{};
		ui32 capsuleCount{};
	};

	class SceneSerializer final
	{
	public:
		static bool save(
			World& world,
			const std::string& filePath
		);

		static SceneLoadResult load(
			World& world,
			const std::string& filePath
		);

		static bool saveLevel(
			World& world,
			const std::string& filePath
		);

		static SceneLoadResult loadLevel(
			World& world,
			const std::string& filePath
		);

		static void clear(
			World& world
		);
	};
}
