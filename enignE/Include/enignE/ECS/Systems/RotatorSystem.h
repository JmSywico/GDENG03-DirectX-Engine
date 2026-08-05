#pragma once

#include "Scene/Scene.h"

namespace enignE::ECS
{
	class RotatorSystem
	{
	public:
		void FixedUpdate(Scene::Scene& scene, float fixedDeltaTime) const;
	};
}
