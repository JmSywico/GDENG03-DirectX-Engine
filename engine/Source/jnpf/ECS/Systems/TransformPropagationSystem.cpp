#include "ECS/Systems/TransformPropagationSystem.h"
#include "Core/JobSystem.h"
#include <algorithm>
#include <unordered_set>

namespace jnpf::ECS
{
	void TransformPropagationSystem::Update(entt::registry& registry, float deltaTime)
	{
		Update(registry, deltaTime, nullptr);
	}

	void TransformPropagationSystem::Update(
		entt::registry& registry,
		float deltaTime,
		Core::JobSystem* jobs)
	{
		UNREFERENCED_PARAMETER(deltaTime);
		m_updatedRenderableTransform = false;
		m_visitedTransformCount = 0;
		auto roots = FindRoots(registry);
		PropagateRoots(registry, roots, jobs);
	}

	void TransformPropagationSystem::UpdateDirty(
		entt::registry& registry,
		const std::vector<entt::entity>& dirtyRoots,
		float deltaTime,
		Core::JobSystem* jobs)
	{
		UNREFERENCED_PARAMETER(deltaTime);
		m_updatedRenderableTransform = false;
		m_visitedTransformCount = 0;
		std::unordered_set<entt::entity> candidates;
		for (const entt::entity entity : dirtyRoots)
		{
			if (!registry.valid(entity))
				continue;
			auto* transform = registry.try_get<Scene::TransformComponent>(entity);
			if (!transform || !transform->IsWorldTransformDirty())
				continue;
			candidates.insert(entity);
		}
		std::vector<entt::entity> roots;
		roots.reserve(candidates.size());
		for (const entt::entity entity : candidates)
		{
			bool coveredByAncestor = false;
			entt::entity ancestor = entity;
			while (const auto* hierarchy = registry.try_get<Scene::HierarchyComponent>(ancestor))
			{
				ancestor = hierarchy->Parent;
				if (ancestor == entt::null || !registry.valid(ancestor)) break;
				if (candidates.contains(ancestor))
				{
					coveredByAncestor = true;
					break;
				}
			}
			if (!coveredByAncestor) roots.push_back(entity);
		}
		PropagateRoots(registry, roots, jobs);
	}

	void TransformPropagationSystem::PropagateRoots(
		entt::registry& registry,
		const std::vector<entt::entity>& roots,
		Core::JobSystem* jobs)
	{
		std::vector<PropagationResult> results(roots.size());
		const auto propagate = [this, &registry, &roots, &results](std::size_t index)
		{
			const entt::entity entity = roots[index];

			DirectX::XMMATRIX parentWorld = DirectX::XMMatrixIdentity();
			if (const auto* hierarchy = registry.try_get<Scene::HierarchyComponent>(entity);
				hierarchy && hierarchy->Parent != entt::null && registry.valid(hierarchy->Parent))
			{
				if (const auto* parentTransform =
					registry.try_get<Scene::TransformComponent>(hierarchy->Parent))
					parentWorld = parentTransform->GetWorldMatrix();
			}
			PropagateTransform(registry, entity, parentWorld, false, results[index]);
		};
		if (jobs && roots.size() >= 64)
			jobs->ParallelFor(roots.size(), 32, propagate);
		else
			for (std::size_t index = 0; index < roots.size(); ++index) propagate(index);
		for (const PropagationResult& result : results)
		{
			m_updatedRenderableTransform = m_updatedRenderableTransform || result.UpdatedRenderable;
			m_visitedTransformCount += result.Visited;
		}
	}

	void TransformPropagationSystem::PropagateTransform(
		entt::registry& registry,
		entt::entity entity,
		const DirectX::XMMATRIX& parentWorldMatrix,
		bool ancestorDirty,
		PropagationResult& result)
	{
		++result.Visited;
		auto* transform = registry.try_get<Scene::TransformComponent>(entity);
		if (!transform)
			return;

		const auto* hierarchy = registry.try_get<Scene::HierarchyComponent>(entity);
		const bool hasChildren = hierarchy && hierarchy->HasChildren();
		const bool isDirty = transform->IsWorldTransformDirty() || ancestorDirty;

		if (!isDirty && !hasChildren)
			return;

		DirectX::XMMATRIX worldMatrix = transform->GetWorldMatrix();
		if (isDirty)
		{
			DirectX::XMMATRIX localMatrix = transform->GetLocalMatrix();
			worldMatrix = DirectX::XMMatrixMultiply(localMatrix, parentWorldMatrix);
			transform->SetWorldMatrix(worldMatrix);
			transform->ClearWorldTransformDirtyFlag();
			result.UpdatedRenderable =
				result.UpdatedRenderable || registry.all_of<Scene::MeshRendererComponent>(entity);
		}

		if (!hasChildren)
			return;

		entt::entity child = hierarchy->FirstChild;
		while (child != entt::null && registry.valid(child))
		{
			entt::entity nextSibling = entt::null;
			if (const auto* childHierarchy = registry.try_get<Scene::HierarchyComponent>(child))
				nextSibling = childHierarchy->NextSibling;

			PropagateTransform(registry, child, worldMatrix, isDirty, result);
			child = nextSibling;
		}
	}

	std::vector<entt::entity> TransformPropagationSystem::FindRoots(entt::registry& registry)
	{
		std::vector<entt::entity> roots;
		auto view = registry.view<Scene::TransformComponent>();
		roots.reserve(view.size());
		for (const entt::entity entity : view)
		{
			const auto* hier = registry.try_get<Scene::HierarchyComponent>(entity);
			if (hier)
			{
				if (hier->IsRoot())
				{
					roots.push_back(entity);
				}
			}
			else
			{
				roots.push_back(entity);
			}
		}

		return roots;
	}
}
