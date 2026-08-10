#pragma once

#include "../../pch.h"
#include "SystemBase.h"
#include "HierarchySystem.h"
#include "../../Scene/Components/TransformComponent.h"
#include <entt/entity/registry.hpp>
#include <vector>

namespace enignE::Core { class JobSystem; }

namespace enignE::ECS
{
	/// \brief Computes cached world matrices from local transforms and hierarchy links.
	class TransformPropagationSystem : public SystemBase
	{
	public:
		TransformPropagationSystem() = default;
		~TransformPropagationSystem() override = default;

		void Update(entt::registry& registry, float deltaTime) override;
		void Update(entt::registry& registry, float deltaTime, Core::JobSystem* jobs);
		void UpdateDirty(
			entt::registry& registry,
			const std::vector<entt::entity>& dirtyRoots,
			float deltaTime,
			Core::JobSystem* jobs = nullptr);
		bool DidUpdateRenderableTransform() const { return m_updatedRenderableTransform; }
		std::size_t GetVisitedTransformCount() const { return m_visitedTransformCount; }

		const char* GetName() const override { return "TransformPropagationSystem"; }

	private:
		struct PropagationResult
		{
			bool UpdatedRenderable = false;
			std::size_t Visited = 0;
		};
		void PropagateTransform(
			entt::registry& registry,
			entt::entity entity,
			const DirectX::XMMATRIX& parentWorldMatrix,
			bool ancestorDirty,
			PropagationResult& result);
		void PropagateRoots(
			entt::registry& registry,
			const std::vector<entt::entity>& roots,
			Core::JobSystem* jobs);

		std::vector<entt::entity> FindRoots(entt::registry& registry);

		bool m_updatedRenderableTransform = false;
		std::size_t m_visitedTransformCount = 0;
	};
}
