#pragma once

#include "../pch.h"
#include "../ECS/Systems/HierarchySystem.h"
#include "Components/TransformComponent.h"
#include <entt/entity/registry.hpp>
#include <vector>
#include <functional>

namespace jnpf::Scene
{
	/// \brief Thin convenience wrapper around ECS hierarchy operations.
	class HierarchyHelpers
	{
	public:
		static entt::entity GetParent(const entt::registry& registry, entt::entity child)
		{
			return ECS::HierarchySystem::GetParent(registry, child);
		}

		static std::vector<entt::entity> GetChildren(const entt::registry& registry, entt::entity parent)
		{
			return ECS::HierarchySystem::GetChildren(registry, parent);
		}

		static uint32_t GetChildCount(const entt::registry& registry, entt::entity parent)
		{
			const auto* hier = registry.valid(parent) ? registry.try_get<HierarchyComponent>(parent) : nullptr;
			return hier ? hier->ChildCount : 0;
		}

		static bool HasParent(const entt::registry& registry, entt::entity entity)
		{
			const auto* hier = registry.valid(entity) ? registry.try_get<HierarchyComponent>(entity) : nullptr;
			return hier && hier->Parent != entt::null;
		}

		static bool HasChildren(const entt::registry& registry, entt::entity entity)
		{
			return GetChildCount(registry, entity) > 0;
		}

		static bool IsRoot(const entt::registry& registry, entt::entity entity)
		{
			return !HasParent(registry, entity);
		}

		static bool IsAncestor(const entt::registry& registry, entt::entity ancestor, entt::entity entity)
		{
			return ECS::HierarchySystem::IsAncestor(registry, ancestor, entity);
		}

		/// \brief Reparents a child without changing its local transform.
		static void SetParent(entt::registry& registry, entt::entity child, entt::entity newParent)
		{
			ECS::HierarchySystem::SetParent(registry, child, newParent);
		}

		static void MakeRoot(entt::registry& registry, entt::entity entity)
		{
			ECS::HierarchySystem::SetParent(registry, entity, entt::null);
		}

		/// \brief Reparents a child and recomputes local transform so world placement is preserved.
		static void ReparentKeepWorldTransform(
			entt::registry& registry,
			entt::entity child,
			entt::entity newParent)
		{
			ECS::HierarchySystem::ReparentKeepWorldTransform(registry, child, newParent);
		}

		/// \brief Reparents a child without recomputing local transform.
		static void ReparentKeepLocalTransform(
			entt::registry& registry,
			entt::entity child,
			entt::entity newParent)
		{
			ECS::HierarchySystem::ReparentKeepLocalTransform(registry, child, newParent);
		}

		static void AddChild(entt::registry& registry, entt::entity parent, entt::entity child)
		{
			ECS::HierarchySystem::AddChild(registry, parent, child);
		}

		static void RemoveChild(entt::registry& registry, entt::entity parent, entt::entity child)
		{
			ECS::HierarchySystem::RemoveChild(registry, parent, child);
		}

		/// \brief Visits descendants recursively, excluding the root entity itself.
		static void TraverseDescendants(
			const entt::registry& registry,
			entt::entity entity,
			const std::function<void(entt::entity)>& callback)
		{
			ECS::HierarchySystem::TraverseDescendants(registry, entity, callback);
		}

		static std::vector<entt::entity> GetAllDescendants(const entt::registry& registry, entt::entity entity)
		{
			std::vector<entt::entity> descendants;
			TraverseDescendants(registry, entity, [&descendants](entt::entity descendant)
			{
				descendants.push_back(descendant);
			});
			return descendants;
		}

		static uint32_t GetDepth(const entt::registry& registry, entt::entity entity)
		{
			uint32_t depth = 0;
			entt::entity current = entity;

			while (current != entt::null && registry.valid(current))
			{
				const auto* hier = registry.try_get<HierarchyComponent>(current);
				if (!hier || hier->Parent == entt::null)
					break;
				depth++;
				current = hier->Parent;
			}

			return depth;
		}

		/// \brief Logs an entity subtree with indentation by depth.
		static void PrintHierarchy(const entt::registry& registry, entt::entity entity, int indent = 0)
		{
			LOG_DEBUGF(
				"{0}- Entity[{1}]",
				std::string(static_cast<std::size_t>(indent * 2), ' '),
				static_cast<std::uint32_t>(entity));

			auto children = GetChildren(registry, entity);
			for (const auto& child : children)
			{
				PrintHierarchy(registry, child, indent + 1);
			}
		}
	};
}
