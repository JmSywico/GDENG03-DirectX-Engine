#pragma once

#include <DirectXMath.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace jnpf::Physics
{
	enum class MotionType : std::uint8_t { Static, Dynamic, Kinematic };
	enum class ShapeType : std::uint8_t { Box, Sphere };

	struct PhysicsWorldConfig
	{
		std::uint32_t MaxBodies = 65536;
		std::uint32_t MaxBodyPairs = 65536;
		std::uint32_t MaxContactConstraints = 10240;
		std::uint32_t WorkerThreads = 1;
		std::size_t TemporaryAllocatorBytes = 10u * 1024u * 1024u;
		DirectX::XMFLOAT3 Gravity{0.0f, -9.81f, 0.0f};
	};

	/**
	 * @brief Backend-neutral configuration used to construct one rigid body.
	 *
	 * EntityID is the only identity crossing the adapter boundary. Box uses
	 * HalfExtents; sphere uses Radius. Values are expressed in meters, radians,
	 * seconds, and kilograms.
	 */
	struct PhysicsBodyConfig
	{
		std::uint64_t EntityID = 0;
		MotionType Motion = MotionType::Static;
		ShapeType Shape = ShapeType::Box;
		DirectX::XMFLOAT3 Position{0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT4 Rotation{0.0f, 0.0f, 0.0f, 1.0f};
		DirectX::XMFLOAT3 HalfExtents{0.5f, 0.5f, 0.5f};
		float Radius = 0.5f;
		float Friction = 0.5f;
		float Restitution = 0.0f;
		float LinearDamping = 0.05f;
		float AngularDamping = 0.05f;
		float GravityFactor = 1.0f;
	};

	struct PhysicsBodyState
	{
		DirectX::XMFLOAT3 Position{0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT4 Rotation{0.0f, 0.0f, 0.0f, 1.0f};
		DirectX::XMFLOAT3 LinearVelocity{0.0f, 0.0f, 0.0f};
		bool Active = false;
	};

	struct PhysicsWorldStats
	{
		std::size_t BodyCount = 0;
		std::size_t ContactCount = 0;
		std::size_t ContactEventCount = 0;
		double LastStepMilliseconds = 0.0;
	};

	enum class ContactEventType : std::uint8_t { Begin, Persist, End };

	struct PhysicsContact
	{
		std::uint64_t EntityA = 0;
		std::uint64_t EntityB = 0;
		DirectX::XMFLOAT3 Position{0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 Normal{0.0f, 1.0f, 0.0f};
		float PenetrationDepth = 0.0f;
		std::uint32_t PointCount = 0;
	};

	struct PhysicsContactEvent
	{
		ContactEventType Type = ContactEventType::Begin;
		PhysicsContact Contact;
	};

	struct PhysicsRaycastHit
	{
		std::uint64_t EntityID = 0;
		DirectX::XMFLOAT3 Position{0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 Normal{0.0f, 1.0f, 0.0f};
		float Distance = 0.0f;
	};

	/**
	 * @brief Engine-owned rigid-body world with all backend types hidden by PIMPL.
	 *
	 * Calls are engine-thread-owned. Step may use bounded backend workers but
	 * joins them before returning, so no ECS state is accessed concurrently.
	 */
	class PhysicsWorld
	{
	public:
		PhysicsWorld();
		~PhysicsWorld();

		PhysicsWorld(const PhysicsWorld&) = delete;
		PhysicsWorld& operator=(const PhysicsWorld&) = delete;

		bool Initialize(const PhysicsWorldConfig& config = {});
		void Shutdown();
		bool IsInitialized() const;

		bool AddBody(const PhysicsBodyConfig& config);
		bool RemoveBody(std::uint64_t entityID);
		bool ContainsBody(std::uint64_t entityID) const;
		bool SetBodyTransform(
			std::uint64_t entityID,
			const DirectX::XMFLOAT3& position,
			const DirectX::XMFLOAT4& rotation,
			float fixedDeltaTime);
		bool GetBodyState(std::uint64_t entityID, PhysicsBodyState& state) const;
		bool Raycast(
			const DirectX::XMFLOAT3& origin,
			const DirectX::XMFLOAT3& direction,
			float maxDistance,
			PhysicsRaycastHit& hit) const;
		bool Step(float fixedDeltaTime);
		PhysicsWorldStats GetStats() const;
		const std::vector<PhysicsContactEvent>& GetContactEvents() const;
		const std::vector<PhysicsContact>& GetActiveContacts() const;

	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
