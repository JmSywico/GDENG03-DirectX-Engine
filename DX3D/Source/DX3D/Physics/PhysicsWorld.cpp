#include <DX3D/Physics/PhysicsWorld.h>

#include <DX3D/Game/World.h>
#include <DX3D/Game/GameObject.h>
#include <DX3D/Component/RigidBodyComponent.h>
#include <DX3D/Component/TransformComponent.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

namespace
{
	struct BodyRecord
	{
		dx3d::RigidBodyComponent* body{};
		dx3d::TransformComponent* transform{};
		dx3d::Vec3 position{};
		dx3d::Vec3 extent{};
	};

	dx3d::Vec3 colliderExtent(dx3d::RigidBodyComponent& body, dx3d::TransformComponent& transform)
	{
		const dx3d::Vec3 scale = transform.getScale();
		if (body.getColliderShape() == dx3d::ColliderShape::Sphere)
		{
			const float radius = body.getRadius() * std::max({ std::fabs(scale.x), std::fabs(scale.y), std::fabs(scale.z) });
			return { radius, radius, radius };
		}
		const dx3d::Vec3 half = body.getHalfExtents();
		return { std::fabs(scale.x) * half.x, std::fabs(scale.y) * half.y, std::fabs(scale.z) * half.z };
	}
}

void dx3d::PhysicsWorld::step(World& world, f32 fixedDeltaTime)
{
	const auto started = std::chrono::steady_clock::now();
	m_stats = {};
	ui32 count = 0;
	auto components = world.getComponents<RigidBodyComponent>(count);
	std::vector<BodyRecord> bodies{};
	bodies.reserve(count);

	for (ui32 index = 0; index < count; ++index)
	{
		auto* body = components[index];
		if (!body) continue;
		auto& object = body->getGameObject();
		auto& transform = object.getTransform();
		bodies.push_back({ body, &transform, transform.getPosition(), colliderExtent(*body, transform) });
		++m_stats.bodyCount;
	}

	for (auto& record : bodies)
	{
		if (record.body->getBodyType() != RigidBodyType::Dynamic) continue;
		++m_stats.activeBodyCount;
		Vec3 velocity = record.body->getLinearVelocity();
		if (record.body->isGravityEnabled()) velocity.y -= 9.81f * fixedDeltaTime;
		record.position.x += velocity.x * fixedDeltaTime;
		record.position.y += velocity.y * fixedDeltaTime;
		record.position.z += velocity.z * fixedDeltaTime;

		for (const auto& obstacle : bodies)
		{
			if (&record == &obstacle || obstacle.body->getBodyType() != RigidBodyType::Static) continue;
			const Vec3 delta{ record.position.x - obstacle.position.x, record.position.y - obstacle.position.y, record.position.z - obstacle.position.z };
			const Vec3 overlap{
				record.extent.x + obstacle.extent.x - std::fabs(delta.x),
				record.extent.y + obstacle.extent.y - std::fabs(delta.y),
				record.extent.z + obstacle.extent.z - std::fabs(delta.z)
			};
			if (overlap.x <= 0.0f || overlap.y <= 0.0f || overlap.z <= 0.0f) continue;
			++m_stats.contactCount;
			if (overlap.y <= overlap.x && overlap.y <= overlap.z)
			{
				const float sign = delta.y >= 0.0f ? 1.0f : -1.0f;
				record.position.y += overlap.y * sign;
				velocity.y = -velocity.y * record.body->getRestitution();
				if (std::fabs(velocity.y) < 0.05f) velocity.y = 0.0f;
			}
			else if (overlap.x <= overlap.z)
			{
				record.position.x += overlap.x * (delta.x >= 0.0f ? 1.0f : -1.0f);
				velocity.x = -velocity.x * record.body->getRestitution();
			}
			else
			{
				record.position.z += overlap.z * (delta.z >= 0.0f ? 1.0f : -1.0f);
				velocity.z = -velocity.z * record.body->getRestitution();
			}
		}

		record.body->setLinearVelocity(velocity);
		record.transform->setPosition(record.position);
	}

	const std::chrono::duration<f32, std::milli> elapsed = std::chrono::steady_clock::now() - started;
	m_stats.stepMilliseconds = elapsed.count();
}

void dx3d::PhysicsWorld::reset(World& world)
{
	ui32 count = 0;
	auto components = world.getComponents<RigidBodyComponent>(count);
	for (ui32 index = 0; index < count; ++index)
		if (components[index]) components[index]->setLinearVelocity({});
	m_stats = {};
}

const dx3d::PhysicsWorld::Stats& dx3d::PhysicsWorld::getStats() const noexcept
{
	return m_stats;
}
