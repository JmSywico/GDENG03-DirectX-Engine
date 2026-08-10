#include "Physics/PhysicsWorld.h"

#include "Logging/Logging.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace jnpf::Physics
{
	namespace
	{
		namespace Layers
		{
			constexpr JPH::ObjectLayer NonMoving = 0;
			constexpr JPH::ObjectLayer Moving = 1;
			constexpr JPH::ObjectLayer Count = 2;
		}

		namespace BroadPhaseLayers
		{
			const JPH::BroadPhaseLayer NonMoving{0};
			const JPH::BroadPhaseLayer Moving{1};
			constexpr JPH::uint Count = 2;
		}

		class BroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface
		{
		public:
			BroadPhaseLayerInterface()
		{
				m_layers[Layers::NonMoving] = BroadPhaseLayers::NonMoving;
				m_layers[Layers::Moving] = BroadPhaseLayers::Moving;
			}

			JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::Count; }

			JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
			{
				return layer < Layers::Count ? m_layers[layer] : BroadPhaseLayers::NonMoving;
			}

		private:
			JPH::BroadPhaseLayer m_layers[Layers::Count];
		};

		class ObjectVsBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
		{
		public:
			bool ShouldCollide(JPH::ObjectLayer object, JPH::BroadPhaseLayer broadPhase) const override
			{
				if (object == Layers::NonMoving)
					return broadPhase == BroadPhaseLayers::Moving;
				return object == Layers::Moving;
			}
		};

		class ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
		{
		public:
			bool ShouldCollide(JPH::ObjectLayer first, JPH::ObjectLayer second) const override
			{
				if (first == Layers::NonMoving)
					return second == Layers::Moving;
				return first == Layers::Moving;
			}
		};

		struct JoltRuntime
		{
			JoltRuntime()
			{
				JPH::RegisterDefaultAllocator();
				JPH::Factory::sInstance = new JPH::Factory();
				JPH::RegisterTypes();
			}

			~JoltRuntime()
			{
				JPH::UnregisterTypes();
				delete JPH::Factory::sInstance;
				JPH::Factory::sInstance = nullptr;
			}
		};

		JoltRuntime& GetJoltRuntime()
		{
			static JoltRuntime runtime;
			return runtime;
		}

		bool IsFinite(const DirectX::XMFLOAT3& value)
		{
			return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
		}

		bool IsFinite(const DirectX::XMFLOAT4& value)
		{
			return std::isfinite(value.x) && std::isfinite(value.y)
				&& std::isfinite(value.z) && std::isfinite(value.w);
		}

		JPH::EMotionType ToJoltMotionType(MotionType motion)
		{
			switch (motion)
			{
			case MotionType::Static: return JPH::EMotionType::Static;
			case MotionType::Dynamic: return JPH::EMotionType::Dynamic;
			case MotionType::Kinematic: return JPH::EMotionType::Kinematic;
			}
			return JPH::EMotionType::Static;
		}

		bool IsValid(MotionType motion)
		{
			return motion == MotionType::Static || motion == MotionType::Dynamic
				|| motion == MotionType::Kinematic;
		}

		bool IsValid(ShapeType shape)
		{
			return shape == ShapeType::Box || shape == ShapeType::Sphere;
		}
	}

	struct PhysicsWorld::Impl
	{
		class ContactCollector final : public JPH::ContactListener
		{
		public:
			void BeginStep()
			{
				const std::scoped_lock lock(m_mutex);
				m_events.clear();
			}

			void FinalizeStep(
				std::vector<PhysicsContactEvent>& events,
				std::vector<PhysicsContact>& activeContacts) const
			{
				const std::scoped_lock lock(m_mutex);
				events = m_events;
				activeContacts.clear();
				activeContacts.reserve(m_active.size());
				for (const auto& [key, contact] : m_active)
				{
					(void)key;
					activeContacts.push_back(contact);
				}
				const auto contactLess = [](const PhysicsContact& first, const PhysicsContact& second)
				{
					if (first.EntityA != second.EntityA) return first.EntityA < second.EntityA;
					if (first.EntityB != second.EntityB) return first.EntityB < second.EntityB;
					if (first.Position.x != second.Position.x) return first.Position.x < second.Position.x;
					if (first.Position.y != second.Position.y) return first.Position.y < second.Position.y;
					return first.Position.z < second.Position.z;
				};
				std::sort(activeContacts.begin(), activeContacts.end(), contactLess);
				std::sort(events.begin(), events.end(), [&contactLess](
					const PhysicsContactEvent& first, const PhysicsContactEvent& second)
				{
					if (contactLess(first.Contact, second.Contact)) return true;
					if (contactLess(second.Contact, first.Contact)) return false;
					return first.Type < second.Type;
				});
			}

			void OnContactAdded(
				const JPH::Body& body1,
				const JPH::Body& body2,
				const JPH::ContactManifold& manifold,
				JPH::ContactSettings&) override
			{
				Record(ContactEventType::Begin, body1, body2, manifold);
			}

			void OnContactPersisted(
				const JPH::Body& body1,
				const JPH::Body& body2,
				const JPH::ContactManifold& manifold,
				JPH::ContactSettings&) override
			{
				Record(ContactEventType::Persist, body1, body2, manifold);
			}

			void OnContactRemoved(const JPH::SubShapeIDPair& pair) override
			{
				const std::scoped_lock lock(m_mutex);
				const auto found = m_active.find(pair);
				if (found == m_active.end())
					return;
				m_events.push_back({ContactEventType::End, found->second});
				m_active.erase(found);
			}

		private:
			void Record(
				ContactEventType type,
				const JPH::Body& body1,
				const JPH::Body& body2,
				const JPH::ContactManifold& manifold)
			{
				PhysicsContact contact;
				contact.EntityA = body1.GetUserData();
				contact.EntityB = body2.GetUserData();
				contact.Normal = {
					manifold.mWorldSpaceNormal.GetX(),
					manifold.mWorldSpaceNormal.GetY(),
					manifold.mWorldSpaceNormal.GetZ()};
				contact.PenetrationDepth = manifold.mPenetrationDepth;
				contact.PointCount = static_cast<std::uint32_t>(
					manifold.mRelativeContactPointsOn1.size());
				if (contact.PointCount > 0)
				{
					DirectX::XMFLOAT3 sum{};
					for (std::uint32_t index = 0; index < contact.PointCount; ++index)
					{
						const JPH::RVec3 point1 = manifold.GetWorldSpaceContactPointOn1(index);
						const JPH::RVec3 point2 = manifold.GetWorldSpaceContactPointOn2(index);
						sum.x += static_cast<float>((point1.GetX() + point2.GetX()) * 0.5);
						sum.y += static_cast<float>((point1.GetY() + point2.GetY()) * 0.5);
						sum.z += static_cast<float>((point1.GetZ() + point2.GetZ()) * 0.5);
					}
					const float inverseCount = 1.0f / static_cast<float>(contact.PointCount);
					contact.Position = {sum.x * inverseCount, sum.y * inverseCount, sum.z * inverseCount};
				}
				else
				{
					contact.Position = {
						static_cast<float>(manifold.mBaseOffset.GetX()),
						static_cast<float>(manifold.mBaseOffset.GetY()),
						static_cast<float>(manifold.mBaseOffset.GetZ())};
				}
				if (contact.EntityB < contact.EntityA)
				{
					std::swap(contact.EntityA, contact.EntityB);
					contact.Normal = {-contact.Normal.x, -contact.Normal.y, -contact.Normal.z};
				}

				const JPH::SubShapeIDPair key(
					body1.GetID(), manifold.mSubShapeID1,
					body2.GetID(), manifold.mSubShapeID2);
				const std::scoped_lock lock(m_mutex);
				m_active[key] = contact;
				m_events.push_back({type, contact});
			}

			mutable std::mutex m_mutex;
			std::unordered_map<JPH::SubShapeIDPair, PhysicsContact> m_active;
			std::vector<PhysicsContactEvent> m_events;
		};

		struct BodyRecord
		{
			JPH::BodyID ID;
			MotionType Motion = MotionType::Static;
		};
		BroadPhaseLayerInterface BroadPhaseLayers;
		ObjectVsBroadPhaseFilter ObjectVsBroadPhase;
		ObjectLayerPairFilter ObjectPairs;
		std::unique_ptr<JPH::TempAllocatorImpl> TempAllocator;
		std::unique_ptr<JPH::JobSystemThreadPool> Jobs;
		std::unique_ptr<JPH::PhysicsSystem> System;
		ContactCollector Contacts;
		std::unordered_map<std::uint64_t, BodyRecord> Bodies;
		std::vector<PhysicsContactEvent> ContactEvents;
		std::vector<PhysicsContact> ActiveContacts;
		PhysicsWorldStats Stats;
	};

	PhysicsWorld::PhysicsWorld() = default;
	PhysicsWorld::~PhysicsWorld() { Shutdown(); }

	bool PhysicsWorld::Initialize(const PhysicsWorldConfig& config)
	{
		Shutdown();
		if (config.MaxBodies == 0 || config.MaxBodyPairs == 0
			|| config.MaxContactConstraints == 0 || config.TemporaryAllocatorBytes == 0
			|| !IsFinite(config.Gravity))
		{
			LOG_ERROR("PhysicsWorld rejected invalid capacity or gravity configuration");
			return false;
		}

		(void)GetJoltRuntime();
		auto impl = std::make_unique<Impl>();
		impl->TempAllocator = std::make_unique<JPH::TempAllocatorImpl>(config.TemporaryAllocatorBytes);
		const std::uint32_t availableWorkers = std::max(1u, std::thread::hardware_concurrency());
		const std::uint32_t workerCount = std::clamp(config.WorkerThreads, 1u, availableWorkers);
		impl->Jobs = std::make_unique<JPH::JobSystemThreadPool>(
			JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, static_cast<int>(workerCount));
		impl->System = std::make_unique<JPH::PhysicsSystem>();
		impl->System->Init(
			config.MaxBodies,
			0,
			config.MaxBodyPairs,
			config.MaxContactConstraints,
			impl->BroadPhaseLayers,
			impl->ObjectVsBroadPhase,
			impl->ObjectPairs);
		impl->System->SetGravity(JPH::Vec3(config.Gravity.x, config.Gravity.y, config.Gravity.z));
		impl->System->SetContactListener(&impl->Contacts);
		m_impl = std::move(impl);
		return true;
	}

	void PhysicsWorld::Shutdown()
	{
		if (!m_impl)
			return;
		JPH::BodyInterface& bodies = m_impl->System->GetBodyInterface();
		for (const auto& [entityID, body] : m_impl->Bodies)
		{
			(void)entityID;
			bodies.RemoveBody(body.ID);
			bodies.DestroyBody(body.ID);
		}
		m_impl->System->SetContactListener(nullptr);
		m_impl.reset();
	}

	bool PhysicsWorld::IsInitialized() const { return m_impl != nullptr; }

	bool PhysicsWorld::AddBody(const PhysicsBodyConfig& config)
	{
		if (!m_impl || config.EntityID == 0 || ContainsBody(config.EntityID)
			|| !IsValid(config.Motion) || !IsValid(config.Shape)
			|| !IsFinite(config.Position) || !IsFinite(config.Rotation)
			|| !std::isfinite(config.Friction) || !std::isfinite(config.Restitution)
			|| !std::isfinite(config.LinearDamping) || !std::isfinite(config.AngularDamping)
			|| !std::isfinite(config.GravityFactor) || config.Friction < 0.0f
			|| config.Restitution < 0.0f || config.Restitution > 1.0f
			|| config.LinearDamping < 0.0f || config.AngularDamping < 0.0f)
		{
			return false;
		}
		const float rotationLengthSquared = config.Rotation.x * config.Rotation.x
			+ config.Rotation.y * config.Rotation.y
			+ config.Rotation.z * config.Rotation.z
			+ config.Rotation.w * config.Rotation.w;
		if (rotationLengthSquared <= std::numeric_limits<float>::epsilon())
			return false;

		JPH::ShapeRefC shape;
		if (config.Shape == ShapeType::Box)
		{
			if (!IsFinite(config.HalfExtents) || config.HalfExtents.x <= 0.0f
				|| config.HalfExtents.y <= 0.0f || config.HalfExtents.z <= 0.0f)
				return false;
			shape = new JPH::BoxShape(JPH::Vec3(
				config.HalfExtents.x, config.HalfExtents.y, config.HalfExtents.z));
		}
		else
		{
			if (!std::isfinite(config.Radius) || config.Radius <= 0.0f)
				return false;
			shape = new JPH::SphereShape(config.Radius);
		}

		JPH::Quat rotation(config.Rotation.x, config.Rotation.y, config.Rotation.z, config.Rotation.w);
		rotation = rotation.Normalized();
		JPH::BodyCreationSettings settings(
			shape,
			JPH::RVec3(config.Position.x, config.Position.y, config.Position.z),
			rotation,
			ToJoltMotionType(config.Motion),
			config.Motion == MotionType::Static ? Layers::NonMoving : Layers::Moving);
		settings.mUserData = config.EntityID;
		settings.mFriction = config.Friction;
		settings.mRestitution = config.Restitution;
		settings.mLinearDamping = config.LinearDamping;
		settings.mAngularDamping = config.AngularDamping;
		settings.mGravityFactor = config.GravityFactor;

		JPH::BodyInterface& bodies = m_impl->System->GetBodyInterface();
		const JPH::BodyID bodyID = bodies.CreateAndAddBody(
			settings,
			config.Motion == MotionType::Dynamic
				? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
		if (bodyID.IsInvalid())
			return false;
		m_impl->Bodies.emplace(config.EntityID, Impl::BodyRecord{bodyID, config.Motion});
		m_impl->Stats.BodyCount = m_impl->Bodies.size();
		return true;
	}

	bool PhysicsWorld::RemoveBody(std::uint64_t entityID)
	{
		if (!m_impl)
			return false;
		const auto found = m_impl->Bodies.find(entityID);
		if (found == m_impl->Bodies.end())
			return false;
		JPH::BodyInterface& bodies = m_impl->System->GetBodyInterface();
		bodies.RemoveBody(found->second.ID);
		bodies.DestroyBody(found->second.ID);
		m_impl->Bodies.erase(found);
		m_impl->Stats.BodyCount = m_impl->Bodies.size();
		return true;
	}

	bool PhysicsWorld::ContainsBody(std::uint64_t entityID) const
	{
		return m_impl && m_impl->Bodies.contains(entityID);
	}

	bool PhysicsWorld::SetBodyTransform(
		std::uint64_t entityID,
		const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT4& rotation,
		float fixedDeltaTime)
	{
		if (!m_impl || !IsFinite(position) || !IsFinite(rotation)
			|| !std::isfinite(fixedDeltaTime) || fixedDeltaTime <= 0.0f)
			return false;
		const auto found = m_impl->Bodies.find(entityID);
		if (found == m_impl->Bodies.end() || found->second.Motion == MotionType::Dynamic)
			return false;
		const float lengthSquared = rotation.x * rotation.x + rotation.y * rotation.y
			+ rotation.z * rotation.z + rotation.w * rotation.w;
		if (lengthSquared <= std::numeric_limits<float>::epsilon())
			return false;
		JPH::Quat orientation(rotation.x, rotation.y, rotation.z, rotation.w);
		orientation = orientation.Normalized();
		JPH::BodyInterface& bodies = m_impl->System->GetBodyInterface();
		const JPH::RVec3 target(position.x, position.y, position.z);
		if (found->second.Motion == MotionType::Kinematic)
			bodies.MoveKinematic(found->second.ID, target, orientation, fixedDeltaTime);
		else
			bodies.SetPositionAndRotation(
				found->second.ID, target, orientation, JPH::EActivation::DontActivate);
		return true;
	}

	bool PhysicsWorld::GetBodyState(std::uint64_t entityID, PhysicsBodyState& state) const
	{
		if (!m_impl)
			return false;
		const auto found = m_impl->Bodies.find(entityID);
		if (found == m_impl->Bodies.end())
			return false;
		const JPH::BodyInterface& bodies = m_impl->System->GetBodyInterface();
		JPH::RVec3 position;
		JPH::Quat rotation;
		bodies.GetPositionAndRotation(found->second.ID, position, rotation);
		const JPH::Vec3 velocity = bodies.GetLinearVelocity(found->second.ID);
		state.Position = {
			static_cast<float>(position.GetX()),
			static_cast<float>(position.GetY()),
			static_cast<float>(position.GetZ())};
		state.Rotation = {rotation.GetX(), rotation.GetY(), rotation.GetZ(), rotation.GetW()};
		state.LinearVelocity = {velocity.GetX(), velocity.GetY(), velocity.GetZ()};
		state.Active = bodies.IsActive(found->second.ID);
		return true;
	}

	bool PhysicsWorld::Raycast(
		const DirectX::XMFLOAT3& origin,
		const DirectX::XMFLOAT3& direction,
		float maxDistance,
		PhysicsRaycastHit& hit) const
	{
		if (!m_impl || !IsFinite(origin) || !IsFinite(direction)
			|| !std::isfinite(maxDistance) || maxDistance <= 0.0f)
			return false;
		const float lengthSquared = direction.x * direction.x
			+ direction.y * direction.y + direction.z * direction.z;
		if (lengthSquared <= std::numeric_limits<float>::epsilon())
			return false;
		const float inverseLength = 1.0f / std::sqrt(lengthSquared);
		const JPH::Vec3 rayDirection(
			direction.x * inverseLength * maxDistance,
			direction.y * inverseLength * maxDistance,
			direction.z * inverseLength * maxDistance);
		const JPH::RRayCast ray(
			JPH::RVec3(origin.x, origin.y, origin.z), rayDirection);
		JPH::RayCastResult result;
		if (!m_impl->System->GetNarrowPhaseQuery().CastRay(ray, result))
			return false;

		const JPH::BodyLockRead lock(m_impl->System->GetBodyLockInterface(), result.mBodyID);
		if (!lock.Succeeded())
			return false;
		const JPH::RVec3 position = ray.GetPointOnRay(result.mFraction);
		const JPH::Vec3 normal = lock.GetBody().GetWorldSpaceSurfaceNormal(
			result.mSubShapeID2, position);
		hit.EntityID = lock.GetBody().GetUserData();
		hit.Position = {
			static_cast<float>(position.GetX()),
			static_cast<float>(position.GetY()),
			static_cast<float>(position.GetZ())};
		hit.Normal = {normal.GetX(), normal.GetY(), normal.GetZ()};
		hit.Distance = result.mFraction * maxDistance;
		return true;
	}

	bool PhysicsWorld::Step(float fixedDeltaTime)
	{
		if (!m_impl || !std::isfinite(fixedDeltaTime) || fixedDeltaTime <= 0.0f)
			return false;
		m_impl->Contacts.BeginStep();
		const auto start = std::chrono::steady_clock::now();
		const JPH::EPhysicsUpdateError error = m_impl->System->Update(
			fixedDeltaTime, 1, m_impl->TempAllocator.get(), m_impl->Jobs.get());
		m_impl->Stats.LastStepMilliseconds = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - start).count();
		if (error != JPH::EPhysicsUpdateError::None)
		{
			LOG_ERRORF("Jolt physics update failed with error mask {}", static_cast<std::uint32_t>(error));
			return false;
		}
		m_impl->Contacts.FinalizeStep(m_impl->ContactEvents, m_impl->ActiveContacts);
		m_impl->Stats.ContactCount = m_impl->ActiveContacts.size();
		m_impl->Stats.ContactEventCount = m_impl->ContactEvents.size();
		return true;
	}

	PhysicsWorldStats PhysicsWorld::GetStats() const
	{
		return m_impl ? m_impl->Stats : PhysicsWorldStats{};
	}

	const std::vector<PhysicsContactEvent>& PhysicsWorld::GetContactEvents() const
	{
		static const std::vector<PhysicsContactEvent> empty;
		return m_impl ? m_impl->ContactEvents : empty;
	}

	const std::vector<PhysicsContact>& PhysicsWorld::GetActiveContacts() const
	{
		static const std::vector<PhysicsContact> empty;
		return m_impl ? m_impl->ActiveContacts : empty;
	}
}
