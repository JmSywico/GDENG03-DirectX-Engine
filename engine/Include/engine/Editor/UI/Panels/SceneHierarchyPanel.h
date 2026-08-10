#pragma once

#include <entt/entity/entity.hpp>

namespace jnpf::Editor
{
	struct EditorContext;

	// Presents scene elements backed by ECS entities.
	class SceneHierarchyPanel
	{
	public:
		void Draw(EditorContext& context);

	private:
		entt::entity m_rangeAnchor = entt::null;
	};
}
