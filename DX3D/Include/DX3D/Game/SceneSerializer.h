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

		// Memory snapshots are used by editor undo/redo and isolated Play Mode.
		// They use exactly the same versioned representation as scene files.
		static std::string serialize(World& world);

		static SceneLoadResult deserialize(
			World& world,
			const std::string& sceneData
		);

		static void clear(
			World& world
		);
	};
}
