#include "ECS/Systems/FlyControllerSystem.h"
#include <algorithm>
#include <cmath>

namespace jnpf::ECS
{
	void FlyControllerSystem::ApplyLook(Scene::Scene& scene, float deltaX, float deltaY) const
	{
		if (!std::isfinite(deltaX) || !std::isfinite(deltaY)) return;
		auto view = scene.GetRegistry().view<Scene::TransformComponent, Scene::FlyControllerComponent>();
		for (const entt::entity entity : view)
		{
			const auto& controller = view.get<Scene::FlyControllerComponent>(entity);
			if (!controller.Enabled) continue;
			auto rotation = view.get<Scene::TransformComponent>(entity).GetLocalRotation();
			rotation.y += deltaX * controller.LookSensitivity;
			const float limit = DirectX::XMConvertToRadians(
				std::clamp(controller.PitchLimitDegrees, 1.0f, 89.9f));
			rotation.x = std::clamp(rotation.x + deltaY * controller.LookSensitivity, -limit, limit);
			scene.SetLocalRotation(entity, rotation);
		}
	}

	void FlyControllerSystem::FixedUpdate(Scene::Scene& scene,
		const Input::InputActionMap& actions, float dt, bool authorized) const
	{
		if (!authorized || !(dt > 0.0f) || !std::isfinite(dt)) return;
		const float forwardInput = actions.GetValue("MoveForward");
		const float rightInput = actions.GetValue("MoveRight");
		const float upInput = actions.GetValue("MoveUp");
		const float boost = actions.IsDown("Boost") ? 1.0f : 0.0f;
		auto view = scene.GetRegistry().view<Scene::TransformComponent, Scene::FlyControllerComponent>();
		for (const entt::entity entity : view)
		{
			const auto& controller = view.get<Scene::FlyControllerComponent>(entity);
			if (!controller.Enabled) continue;
			const auto& transform = view.get<Scene::TransformComponent>(entity);
			const auto rotation = transform.GetLocalRotation();
			const DirectX::XMMATRIX orientation = DirectX::XMMatrixRotationRollPitchYaw(
				rotation.x, rotation.y, rotation.z);
			DirectX::XMVECTOR direction = DirectX::XMVectorZero();
			direction = DirectX::XMVectorAdd(direction, DirectX::XMVectorScale(
				DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0, 0, 1, 0), orientation), forwardInput));
			direction = DirectX::XMVectorAdd(direction, DirectX::XMVectorScale(
				DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(1, 0, 0, 0), orientation), rightInput));
			direction = DirectX::XMVectorAdd(direction, DirectX::XMVectorSet(0, upInput, 0, 0));
			const float lengthSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(direction));
			if (lengthSq <= 0.000001f) continue;
			direction = DirectX::XMVector3Normalize(direction);
			const float speed = controller.MoveSpeed
				* (boost > 0.0f ? controller.BoostMultiplier : 1.0f) * dt;
			DirectX::XMVECTOR position = DirectX::XMLoadFloat3(&transform.GetLocalPosition());
			position = DirectX::XMVectorAdd(position, DirectX::XMVectorScale(direction, speed));
			DirectX::XMFLOAT3 result{};
			DirectX::XMStoreFloat3(&result, position);
			scene.SetLocalPosition(entity, result);
		}
	}
}
