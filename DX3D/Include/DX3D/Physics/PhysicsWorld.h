#pragma once

#include <DX3D/Core/Core.h>
#include <DX3D/Math/Vec3.h>

#include <memory>
#include <vector>

namespace dx3d
{
	class World;

	enum class PhysicsContactEventType : ui32 { Begin, Persist, End };

	struct PhysicsContact
	{
		ui64 entityA{};
		ui64 entityB{};
		Vec3 position{};
		Vec3 normal{ 0.0f, 1.0f, 0.0f };
		f32 penetrationDepth{};
		ui32 pointCount{};
	};

	struct PhysicsContactEvent
	{
		PhysicsContactEventType type{ PhysicsContactEventType::Begin };
		PhysicsContact contact{};
	};

	struct PhysicsRaycastHit
	{
		ui64 entityId{};
		Vec3 position{};
		Vec3 normal{ 0.0f, 1.0f, 0.0f };
		f32 distance{};
	};

	class PhysicsWorld final
	{
	public:
		struct Stats
		{
			ui32 bodyCount{};
			ui32 activeBodyCount{};
			ui32 contactCount{};
			ui32 contactEventCount{};
			ui32 workerThreadCount{};
			f32 stepMilliseconds{};
		};

		PhysicsWorld();
		~PhysicsWorld();
		PhysicsWorld(const PhysicsWorld&) = delete;
		PhysicsWorld& operator=(const PhysicsWorld&) = delete;

		void step(World& world, f32 fixedDeltaTime);
		void reset(World& world);
		bool raycast(const Vec3& origin, const Vec3& direction, f32 maxDistance,
			PhysicsRaycastHit& hit) const;
		const Stats& getStats() const noexcept;
		const std::vector<PhysicsContactEvent>& getContactEvents() const noexcept;
		const std::vector<PhysicsContact>& getActiveContacts() const noexcept;

	private:
		void shutdown();
		struct Impl;
		std::unique_ptr<Impl> m_impl{};
		Stats m_stats{};
	};
}
