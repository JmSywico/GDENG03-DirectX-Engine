#pragma once

#include <DX3D/Game/GameObject.h>
#include <DX3D/Component/RotatorComponent.h>
#include <DX3D/Component/FlyControllerComponent.h>
#include <DX3D/Component/RigidBodyComponent.h>
#include <DX3D/Component/ColliderComponent.h>
#include <DX3D/Component/TextureComponent.h>

#include <array>

namespace dx3d
{
	enum class ComponentKind : ui32 { Rotator, FlyController, RigidBody, Collider, Texture };

	struct ComponentDescriptor
	{
		ComponentKind kind{};
		const char* name{};
		const char* category{};
		bool removable{ true };
		bool serializable{ true };
	};

	class ComponentCatalog final
	{
	public:
		static const std::array<ComponentDescriptor, 5>& descriptors()
		{
			static constexpr std::array values{
				ComponentDescriptor{ ComponentKind::Rotator, "Rotator", "Simulation" },
				ComponentDescriptor{ ComponentKind::FlyController, "Fly Controller", "Simulation" },
				ComponentDescriptor{ ComponentKind::RigidBody, "Rigid Body", "Physics" },
				ComponentDescriptor{ ComponentKind::Collider, "Collider", "Physics" },
				ComponentDescriptor{ ComponentKind::Texture, "Texture", "Rendering" }
			};
			return values;
		}

		static bool has(GameObject& object, ComponentKind kind)
		{
			switch (kind)
			{
			case ComponentKind::Rotator: return object.getComponent<RotatorComponent>() != nullptr;
			case ComponentKind::FlyController: return object.getComponent<FlyControllerComponent>() != nullptr;
			case ComponentKind::RigidBody: return object.getComponent<RigidBodyComponent>() != nullptr;
			case ComponentKind::Collider: return object.getComponent<ColliderComponent>() != nullptr;
			case ComponentKind::Texture: return object.getComponent<TextureComponent>() != nullptr;
			}
			return false;
		}

		static bool addDefault(GameObject& object, ComponentKind kind)
		{
			if (has(object, kind)) return false;
			switch (kind)
			{
			case ComponentKind::Rotator: return object.createOrGetComponent<RotatorComponent>() != nullptr;
			case ComponentKind::FlyController: return object.createOrGetComponent<FlyControllerComponent>() != nullptr;
			case ComponentKind::RigidBody: return object.createOrGetComponent<RigidBodyComponent>() != nullptr;
			case ComponentKind::Collider: return object.createOrGetComponent<ColliderComponent>() != nullptr;
			case ComponentKind::Texture: return object.createOrGetComponent<TextureComponent>() != nullptr;
			}
			return false;
		}

		static bool remove(GameObject& object, ComponentKind kind)
		{
			switch (kind)
			{
			case ComponentKind::Rotator: return object.removeComponent<RotatorComponent>();
			case ComponentKind::FlyController: return object.removeComponent<FlyControllerComponent>();
			case ComponentKind::RigidBody: return object.removeComponent<RigidBodyComponent>();
			case ComponentKind::Collider: return object.removeComponent<ColliderComponent>();
			case ComponentKind::Texture: return object.removeComponent<TextureComponent>();
			}
			return false;
		}
	};
}
