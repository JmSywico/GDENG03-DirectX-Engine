#pragma once

#include "Scene.h"

#include <array>
#include <string_view>

namespace enignE::Scene
{
	enum class ComponentKind : std::uint8_t { Rotator, FlyController, RigidBody, Collider };

	struct ComponentDescriptor
	{
		ComponentKind Kind;
		std::string_view Name;
		std::string_view Category;
		bool Persisted;
		bool Snapshotted;
	};

	class ComponentCatalog
	{
	public:
		static const std::array<ComponentDescriptor, 4>& Descriptors();
		static const ComponentDescriptor& Describe(ComponentKind kind);
		static bool Has(const Scene& scene, entt::entity entity, ComponentKind kind);
		static bool AddDefault(Scene& scene, entt::entity entity, ComponentKind kind);
		static bool Remove(Scene& scene, entt::entity entity, ComponentKind kind);
	};
}
