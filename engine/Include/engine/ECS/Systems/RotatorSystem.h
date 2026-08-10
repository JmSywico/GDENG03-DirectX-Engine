#pragma once

#include "Scene/Scene.h"

namespace jnpf::ECS
{
	class RotatorSystem
	{
	public:
		void FixedUpdate(Scene::Scene& scene, float fixedDeltaTime) const;
	};
}
