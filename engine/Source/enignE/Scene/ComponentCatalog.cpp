#include "Scene/ComponentCatalog.h"

namespace enignE::Scene
{
	const std::array<ComponentDescriptor, 4>& ComponentCatalog::Descriptors()
	{
		static constexpr std::array descriptors{
			ComponentDescriptor{ComponentKind::Rotator, "Rotator", "Simulation", true, true},
			ComponentDescriptor{ComponentKind::FlyController, "Fly Controller", "Simulation", true, true},
			ComponentDescriptor{ComponentKind::RigidBody, "Rigid Body", "Physics", true, true},
			ComponentDescriptor{ComponentKind::Collider, "Collider", "Physics", true, true}
		};
		return descriptors;
	}

	const ComponentDescriptor& ComponentCatalog::Describe(ComponentKind kind)
	{
		return Descriptors().at(static_cast<std::size_t>(kind));
	}

	bool ComponentCatalog::Has(const Scene& scene, entt::entity entity, ComponentKind kind)
	{
		switch (kind)
		{
		case ComponentKind::Rotator: return scene.HasComponent<RotatorComponent>(entity);
		case ComponentKind::FlyController: return scene.HasComponent<FlyControllerComponent>(entity);
		case ComponentKind::RigidBody: return scene.HasComponent<RigidBodyComponent>(entity);
		case ComponentKind::Collider: return scene.HasComponent<ColliderComponent>(entity);
		}
		return false;
	}

	bool ComponentCatalog::AddDefault(Scene& scene, entt::entity entity, ComponentKind kind)
	{
		if (!scene.GetRegistry().valid(entity) || Has(scene, entity, kind)) return false;
		switch (kind)
		{
		case ComponentKind::Rotator:
			scene.AddComponent<RotatorComponent>(entity);
			return true;
		case ComponentKind::FlyController:
			scene.AddComponent<FlyControllerComponent>(entity);
			return true;
		case ComponentKind::RigidBody:
			scene.AddComponent<RigidBodyComponent>(entity);
			return true;
		case ComponentKind::Collider:
			scene.AddComponent<ColliderComponent>(entity);
			return true;
		}
		return false;
	}

	bool ComponentCatalog::Remove(Scene& scene, entt::entity entity, ComponentKind kind)
	{
		switch (kind)
		{
		case ComponentKind::Rotator: return scene.RemoveComponent<RotatorComponent>(entity);
		case ComponentKind::FlyController: return scene.RemoveComponent<FlyControllerComponent>(entity);
		case ComponentKind::RigidBody: return scene.RemoveComponent<RigidBodyComponent>(entity);
		case ComponentKind::Collider: return scene.RemoveComponent<ColliderComponent>(entity);
		}
		return false;
	}
}
