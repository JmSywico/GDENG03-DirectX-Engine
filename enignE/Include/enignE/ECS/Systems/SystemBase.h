#pragma once

#include <entt/entity/registry.hpp>

namespace enignE::ECS
{
	class SystemBase
	{
	public:
		virtual ~SystemBase() = default;
		virtual void Update(entt::registry& registry, float deltaTime) = 0;
		virtual const char* GetName() const { return "SystemBase"; }
	};
}
