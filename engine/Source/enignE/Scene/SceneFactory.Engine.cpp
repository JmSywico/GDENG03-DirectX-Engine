#include "Scene/SceneFactory.h"
#include "Engine.h"

namespace enignE::Scene
{
	entt::entity CreatePrimitive(
		Engine& engine,
		Scene& scene,
		const PrimitiveDesc& desc)
	{
		return CreatePrimitive(
			scene,
			desc,
			[&engine](const MeshData& data)
			{
				return engine.CreateModel(data);
			});
	}
}
