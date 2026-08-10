#include "ECS/Systems/RotatorSystem.h"

#include <cmath>

namespace enignE::ECS
{
	void RotatorSystem::FixedUpdate(Scene::Scene& scene, float fixedDeltaTime) const
	{
		if (!(fixedDeltaTime > 0.0f) || !std::isfinite(fixedDeltaTime)) return;
		auto view = scene.GetRegistry().view<Scene::TransformComponent, Scene::RotatorComponent>();
		for (const entt::entity entity : view)
		{
			const auto& rotator = view.get<Scene::RotatorComponent>(entity);
			if (!rotator.Enabled) continue;
			const auto& rotation = view.get<Scene::TransformComponent>(entity).GetLocalRotation();
			scene.SetLocalRotation(entity, {
				rotation.x + rotator.AngularVelocity.x * fixedDeltaTime,
				rotation.y + rotator.AngularVelocity.y * fixedDeltaTime,
				rotation.z + rotator.AngularVelocity.z * fixedDeltaTime});
		}
	}
}
