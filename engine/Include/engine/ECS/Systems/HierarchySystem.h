#pragma once

#include "../../pch.h"
#include "SystemBase.h"
#include "../../Scene/Components/TransformComponent.h"
#include <entt/entity/registry.hpp>
#include <vector>

namespace jnpf::ECS
{
	/// \brief Maintains parent-child links and marks moved subtrees dirty.
	class HierarchySystem : public SystemBase
	{
	public:
		HierarchySystem() = default;
		~HierarchySystem() override = default;

		void Update(entt::registry& registry, float deltaTime) override;

		const char* GetName() const override { return "HierarchySystem"; }

		/// \brief Reparents a child without changing its local transform.
		static void SetParent(entt::registry& registry, entt::entity child, entt::entity newParent)
		{
			ReparentKeepLocalTransform(registry, child, newParent);
		}

		/// \brief Reparents a child and recomputes local transform so world placement is preserved.
		static void ReparentKeepWorldTransform(entt::registry& registry, entt::entity child, entt::entity newParent);

		static void ReparentKeepLocalTransform(entt::registry& registry, entt::entity child, entt::entity newParent);

		static void AddChild(entt::registry& registry, entt::entity parent, entt::entity child)
		{
			SetParent(registry, child, parent);
		}

		static void RemoveChild(entt::registry& registry, entt::entity parent, entt::entity child);

		static entt::entity GetParent(const entt::registry& registry, entt::entity entity);

		static std::vector<entt::entity> GetChildren(const entt::registry& registry, entt::entity entity);

		/// \brief Visits descendants recursively, excluding the root entity itself.
		static void TraverseDescendants(
			const entt::registry& registry,
			entt::entity entity,
			const std::function<void(entt::entity)>& callback);

		static bool IsAncestor(const entt::registry& registry, entt::entity ancestor, entt::entity entity);

	private:
		static void DetachChild(entt::registry& registry, entt::entity child);

		static void AttachChild(entt::registry& registry, entt::entity parent, entt::entity child);

		static void MarkSubtreeDirty(entt::registry& registry, entt::entity entity);
	};
}
