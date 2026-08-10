#pragma once
#include "Input/InputActions.h"
#include "Scene/Scene.h"

namespace jnpf::ECS
{
	class FlyControllerSystem
	{
	public:
		void ApplyLook(Scene::Scene& scene, float deltaX, float deltaY) const;
		void FixedUpdate(Scene::Scene& scene, const Input::InputActionMap& actions,
			float fixedDeltaTime, bool inputAuthorized) const;
	};
}
