#pragma once

#include "../pch.h"
#include "ECS/Systems/HierarchySystem.h"
#include "ECS/Systems/TransformPropagationSystem.h"
#include "Scene/Components/TransformComponent.h"
#include "Scene/Scene.h"

#include <cassert>

namespace jnpf::Tests::ECS
{
	inline void RunAllTests()
	{
		Scene::Scene scene("EnTTIntegration");
		auto& registry = scene.GetRegistry();

		const entt::entity parent = scene.CreateEntity("Parent");
		const entt::entity child = scene.CreateEntity("Child");
		assert(registry.valid(parent));
		assert(registry.valid(child));

		auto& parentTransform = registry.emplace<Scene::TransformComponent>(parent);
		parentTransform.SetLocalPosition({5.0f, 0.0f, 0.0f});
		auto& childTransform = registry.emplace<Scene::TransformComponent>(child);
		childTransform.SetLocalPosition({1.0f, 0.0f, 0.0f});
		registry.emplace<Scene::HierarchyComponent>(parent);
		registry.emplace<Scene::HierarchyComponent>(child);

		jnpf::ECS::HierarchySystem::SetParent(registry, child, parent);
		assert(jnpf::ECS::HierarchySystem::GetParent(registry, child) == parent);

		jnpf::ECS::TransformPropagationSystem transformSystem;
		transformSystem.Update(registry, 0.0f);

		DirectX::XMFLOAT4X4 childWorld;
		DirectX::XMStoreFloat4x4(&childWorld, registry.get<Scene::TransformComponent>(child).GetWorldMatrix());
		assert(childWorld._41 == 6.0f);

		auto view = registry.view<Scene::TransformComponent, Scene::HierarchyComponent>();
		assert(view.size_hint() == 2);

		scene.DestroyEntity(parent);
		scene.FlushDestroyQueue();
		assert(!registry.valid(parent));
		assert(!registry.valid(child));
		assert(scene.GetEntityCount() == 0);

		LOG_INFO("EnTT ECS integration tests passed");
	}
}
