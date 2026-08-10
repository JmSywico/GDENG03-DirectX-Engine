#include <DX3D/Physics/PhysicsWorld.h>

#include <DX3D/Game/World.h>
#include <DX3D/Game/GameObject.h>
#include <DX3D/Component/RigidBodyComponent.h>
#include <DX3D/Component/ColliderComponent.h>
#include <DX3D/Component/TransformComponent.h>
#include <DX3D/Core/Logger.h>

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <thread>
#include <unordered_map>

#include <Windows.h>
#ifdef interface
#undef interface
#endif

namespace
{
	void joltTrace(const char* format, ...)
	{
		char message[2048]{};
		va_list arguments;
		va_start(arguments, format);
		vsnprintf_s(message, sizeof(message), _TRUNCATE, format, arguments);
		va_end(arguments);
		OutputDebugStringA("[Jolt] ");
		OutputDebugStringA(message);
		OutputDebugStringA("\n");
		std::fprintf(stderr, "[Jolt] %s\n", message);
	}

	namespace Layers
	{
		constexpr JPH::ObjectLayer NonMoving = 0;
		constexpr JPH::ObjectLayer Moving = 1;
		constexpr JPH::uint Count = 2;
	}
	namespace BroadLayers
	{
		const JPH::BroadPhaseLayer NonMoving{ 0 };
		const JPH::BroadPhaseLayer Moving{ 1 };
		constexpr JPH::uint Count = 2;
	}

	class BroadPhaseInterface final : public JPH::BroadPhaseLayerInterface
	{
	public:
		BroadPhaseInterface()
		{
			m_layers[Layers::NonMoving] = BroadLayers::NonMoving;
			m_layers[Layers::Moving] = BroadLayers::Moving;
		}
		JPH::uint GetNumBroadPhaseLayers() const override { return BroadLayers::Count; }
		JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
		{
			return layer < Layers::Count ? m_layers[layer] : BroadLayers::NonMoving;
		}
	private:
		JPH::BroadPhaseLayer m_layers[Layers::Count];
	};

	class ObjectVsBroadPhase final : public JPH::ObjectVsBroadPhaseLayerFilter
	{
	public:
		bool ShouldCollide(JPH::ObjectLayer object, JPH::BroadPhaseLayer broad) const override
		{
			if (object == Layers::NonMoving) return broad == BroadLayers::Moving;
			return object == Layers::Moving;
		}
	};

	class ObjectPairs final : public JPH::ObjectLayerPairFilter
	{
	public:
		bool ShouldCollide(JPH::ObjectLayer first, JPH::ObjectLayer second) const override
		{
			if (first == Layers::NonMoving) return second == Layers::Moving;
			return first == Layers::Moving;
		}
	};

	struct JoltRuntime
	{
		JoltRuntime()
		{
			JPH::RegisterDefaultAllocator();
			JPH::Trace = joltTrace;
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

	JoltRuntime& runtime()
	{
		static JoltRuntime value;
		return value;
	}

	JPH::EMotionType motionType(dx3d::RigidBodyType value)
	{
		switch (value)
		{
		case dx3d::RigidBodyType::Dynamic: return JPH::EMotionType::Dynamic;
		case dx3d::RigidBodyType::Kinematic: return JPH::EMotionType::Kinematic;
		default: return JPH::EMotionType::Static;
		}
	}

	bool isFinite(const dx3d::Vec3& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y)
			&& std::isfinite(value.z);
	}

	bool isFinite(const dx3d::Vec4& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y)
			&& std::isfinite(value.z) && std::isfinite(value.w);
	}

	bool isValid(dx3d::RigidBodyType value)
	{
		return value == dx3d::RigidBodyType::Static
			|| value == dx3d::RigidBodyType::Dynamic
			|| value == dx3d::RigidBodyType::Kinematic;
	}

	bool isValid(dx3d::ColliderShape value)
	{
		return value == dx3d::ColliderShape::Box
			|| value == dx3d::ColliderShape::Sphere
			|| value == dx3d::ColliderShape::Cylinder
			|| value == dx3d::ColliderShape::Capsule;
	}
}

struct dx3d::PhysicsWorld::Impl
{
	class Contacts final : public JPH::ContactListener
	{
	public:
		void beginStep()
		{
			const std::scoped_lock lock(m_mutex);
			events.clear();
		}

		void OnContactAdded(const JPH::Body& a, const JPH::Body& b,
			const JPH::ContactManifold& manifold, JPH::ContactSettings&) override
		{
			record(PhysicsContactEventType::Begin, a, b, manifold);
		}
		void OnContactPersisted(const JPH::Body& a, const JPH::Body& b,
			const JPH::ContactManifold& manifold, JPH::ContactSettings&) override
		{
			record(PhysicsContactEventType::Persist, a, b, manifold);
		}
		void OnContactRemoved(const JPH::SubShapeIDPair& pair) override
		{
			const std::scoped_lock lock(m_mutex);
			const auto found = active.find(pair);
			if (found == active.end()) return;
			events.push_back({ PhysicsContactEventType::End, found->second });
			active.erase(found);
		}

		void snapshot(std::vector<PhysicsContactEvent>& outputEvents,
			std::vector<PhysicsContact>& outputContacts) const
		{
			const std::scoped_lock lock(m_mutex);
			outputEvents = events;
			outputContacts.clear();
			outputContacts.reserve(active.size());
			for (const auto& [key, contact] : active)
			{
				(void)key;
				outputContacts.push_back(contact);
			}
		}

	private:
		void record(PhysicsContactEventType type, const JPH::Body& a,
			const JPH::Body& b, const JPH::ContactManifold& manifold)
		{
			PhysicsContact contact{};
			contact.entityA = a.GetUserData();
			contact.entityB = b.GetUserData();
			contact.normal = { manifold.mWorldSpaceNormal.GetX(),
				manifold.mWorldSpaceNormal.GetY(), manifold.mWorldSpaceNormal.GetZ() };
			contact.penetrationDepth = manifold.mPenetrationDepth;
			contact.pointCount = static_cast<ui32>(manifold.mRelativeContactPointsOn1.size());
			if (contact.pointCount)
			{
				for (ui32 index = 0; index < contact.pointCount; ++index)
				{
					const JPH::RVec3 p1 = manifold.GetWorldSpaceContactPointOn1(index);
					const JPH::RVec3 p2 = manifold.GetWorldSpaceContactPointOn2(index);
					contact.position.x += static_cast<f32>((p1.GetX() + p2.GetX()) * 0.5);
					contact.position.y += static_cast<f32>((p1.GetY() + p2.GetY()) * 0.5);
					contact.position.z += static_cast<f32>((p1.GetZ() + p2.GetZ()) * 0.5);
				}
				const f32 inverse = 1.0f / static_cast<f32>(contact.pointCount);
				contact.position = contact.position * inverse;
			}
			if (contact.entityB < contact.entityA)
			{
				std::swap(contact.entityA, contact.entityB);
				contact.normal = contact.normal * -1.0f;
			}
			const JPH::SubShapeIDPair key(a.GetID(), manifold.mSubShapeID1,
				b.GetID(), manifold.mSubShapeID2);
			const std::scoped_lock lock(m_mutex);
			active[key] = contact;
			events.push_back({ type, contact });
		}

		mutable std::mutex m_mutex{};
		std::unordered_map<JPH::SubShapeIDPair, PhysicsContact> active{};
		std::vector<PhysicsContactEvent> events{};
	};

	struct BodyRecord
	{
		JPH::BodyID id{};
		RigidBodyType motion{ RigidBodyType::Static };
	};

	BroadPhaseInterface broadPhase{};
	ObjectVsBroadPhase objectVsBroad{};
	ObjectPairs pairs{};
	std::unique_ptr<JPH::TempAllocatorImpl> allocator{};
	std::unique_ptr<JPH::JobSystemThreadPool> jobs{};
	std::unique_ptr<JPH::PhysicsSystem> system{};
	Contacts contacts{};
	std::unordered_map<ui64, BodyRecord> bodies{};
	std::vector<PhysicsContactEvent> contactEvents{};
	std::vector<PhysicsContact> activeContacts{};
};

dx3d::PhysicsWorld::PhysicsWorld() = default;

dx3d::PhysicsWorld::~PhysicsWorld()
{
	shutdown();
}

void dx3d::PhysicsWorld::shutdown()
{
	if (!m_impl) return;
	auto& interface = m_impl->system->GetBodyInterface();
	for (const auto& [entity, body] : m_impl->bodies)
	{
		(void)entity;
		interface.RemoveBody(body.id);
		interface.DestroyBody(body.id);
	}
	m_impl->system->SetContactListener(nullptr);
	m_impl.reset();
}

void dx3d::PhysicsWorld::reset(World& world)
{
	shutdown();
	m_stats = {};
	(void)runtime();
	auto impl = std::make_unique<Impl>();
	impl->allocator = std::make_unique<JPH::TempAllocatorImpl>(10u * 1024u * 1024u);
	impl->jobs = std::make_unique<JPH::JobSystemThreadPool>(
		JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, 1);
	impl->system = std::make_unique<JPH::PhysicsSystem>();
	impl->system->Init(65536, 0, 65536, 10240,
		impl->broadPhase, impl->objectVsBroad, impl->pairs);
	impl->system->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));
	impl->system->SetContactListener(&impl->contacts);

	ui32 count = 0;
	auto bodies = world.getComponents<RigidBodyComponent>(count);
	std::vector<RigidBodyComponent*> orderedBodies{};
	orderedBodies.reserve(count);
	for (ui32 index = 0; index < count; ++index)
		if (bodies[index]) orderedBodies.push_back(bodies[index]);
	std::sort(orderedBodies.begin(), orderedBodies.end(),
		[](RigidBodyComponent* first, RigidBodyComponent* second)
		{
			return first->getGameObject().getEntityId()
				< second->getGameObject().getEntityId();
		});

	for (auto* body : orderedBodies)
	{
		if (!body->isEnabled()) continue;
		auto& object = body->getGameObject();
		if (!object.isActiveInHierarchy()) continue;
		auto* collider = object.getComponent<ColliderComponent>();
		if (!collider) continue;
		if (body->getBodyType() == RigidBodyType::Dynamic && object.getParent()) continue;

		const RigidBodyType bodyType = body->getBodyType();
		const ColliderShape colliderShape = collider->getShape();
		auto& transform = object.getTransform();
		const Vec4 rotation = transform.getRotationQuaternion();
		const Vec4 worldPosition = transform.getRigidWorldMatrix().row(3);
		const f32 rotationLengthSquared = rotation.x * rotation.x
			+ rotation.y * rotation.y + rotation.z * rotation.z
			+ rotation.w * rotation.w;
		const bool validSettings = object.getEntityId() != 0
			&& isValid(bodyType) && isValid(colliderShape)
			&& isFinite(rotation) && isFinite(worldPosition)
			&& std::isfinite(rotationLengthSquared)
			&& rotationLengthSquared > 0.000001f
			&& std::isfinite(body->getFriction()) && body->getFriction() >= 0.0f
			&& std::isfinite(body->getRestitution())
			&& body->getRestitution() >= 0.0f && body->getRestitution() <= 1.0f
			&& std::isfinite(body->getLinearDamping()) && body->getLinearDamping() >= 0.0f
			&& std::isfinite(body->getAngularDamping()) && body->getAngularDamping() >= 0.0f
			&& std::isfinite(body->getGravityFactor());
		if (!validSettings)
		{
			DX3DLog(body->getLogger(), Logger::LogLevel::Warning,
				"Physics skipped entity {} ('{}'): invalid rigid-body transform or settings.",
				object.getEntityId(), object.getName());
			continue;
		}

		JPH::ShapeRefC shape{};
		if (colliderShape == ColliderShape::Sphere)
		{
			const f32 radius = collider->getRadius();
			if (!std::isfinite(radius) || radius <= 0.0f)
			{
				DX3DLog(body->getLogger(), Logger::LogLevel::Warning,
					"Physics skipped entity {} ('{}'): sphere radius must be finite and positive.",
					object.getEntityId(), object.getName());
				continue;
			}
			shape = new JPH::SphereShape(radius);
		}
		else if (colliderShape == ColliderShape::Cylinder ||
			colliderShape == ColliderShape::Capsule)
		{
			const f32 radius = collider->getRadius();
			const f32 halfHeight = collider->getHalfExtents().y;
			if (!std::isfinite(radius) || !std::isfinite(halfHeight) ||
				radius <= 0.0f || halfHeight <= 0.0f)
			{
				DX3DLog(body->getLogger(), Logger::LogLevel::Warning,
					"Physics skipped entity {} ('{}'): collider radius and half-height must be finite and positive.",
					object.getEntityId(), object.getName());
				continue;
			}
			shape = colliderShape == ColliderShape::Cylinder
				? JPH::ShapeRefC(new JPH::CylinderShape(halfHeight, radius))
				: JPH::ShapeRefC(new JPH::CapsuleShape(halfHeight, radius));
		}
		else
		{
			const Vec3 half = collider->getHalfExtents();
			if (!isFinite(half) || half.x <= 0.0f || half.y <= 0.0f || half.z <= 0.0f)
			{
				DX3DLog(body->getLogger(), Logger::LogLevel::Warning,
					"Physics skipped entity {} ('{}'): box half-extents must be finite and positive.",
					object.getEntityId(), object.getName());
				continue;
			}
			shape = new JPH::BoxShape(JPH::Vec3(half.x, half.y, half.z));
		}

		JPH::BodyCreationSettings settings(
			shape,
			JPH::RVec3(worldPosition.x, worldPosition.y, worldPosition.z),
			JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w).Normalized(),
			motionType(bodyType),
			bodyType == RigidBodyType::Static ? Layers::NonMoving : Layers::Moving);
		settings.mUserData = object.getEntityId();
		settings.mFriction = body->getFriction();
		settings.mRestitution = body->getRestitution();
		settings.mLinearDamping = body->getLinearDamping();
		settings.mAngularDamping = body->getAngularDamping();
		settings.mGravityFactor = body->getGravityFactor();
		auto& interface = impl->system->GetBodyInterface();
		const JPH::BodyID id = interface.CreateAndAddBody(settings,
			bodyType == RigidBodyType::Dynamic
				? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
		if (!id.IsInvalid())
			impl->bodies.emplace(object.getEntityId(), Impl::BodyRecord{ id, bodyType });
		else
			DX3DLog(body->getLogger(), Logger::LogLevel::Warning,
				"Physics could not create a Jolt body for entity {} ('{}').",
				object.getEntityId(), object.getName());
	}
	m_stats.bodyCount = static_cast<ui32>(impl->bodies.size());
	m_impl = std::move(impl);
}

void dx3d::PhysicsWorld::step(World& world, f32 fixedDeltaTime)
{
	if (!m_impl || fixedDeltaTime <= 0.0f || !std::isfinite(fixedDeltaTime)) return;
	auto& interface = m_impl->system->GetBodyInterface();
	for (const auto& [entityId, record] : m_impl->bodies)
	{
		if (record.motion == RigidBodyType::Dynamic) continue;
		auto* object = world.findGameObject(entityId);
		if (!object) continue;
		auto& transform = object->getTransform();
		const Vec4 position = transform.getRigidWorldMatrix().row(3);
		const Vec4 rotation = transform.getRotationQuaternion();
		const f32 rotationLengthSquared = rotation.x * rotation.x
			+ rotation.y * rotation.y + rotation.z * rotation.z
			+ rotation.w * rotation.w;
		if (!isFinite(position) || !isFinite(rotation)
			|| !std::isfinite(rotationLengthSquared)
			|| rotationLengthSquared <= 0.000001f)
			continue;
		const JPH::RVec3 target(position.x, position.y, position.z);
		const JPH::Quat orientation = JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w).Normalized();
		if (record.motion == RigidBodyType::Kinematic)
			interface.MoveKinematic(record.id, target, orientation, fixedDeltaTime);
		else
			interface.SetPositionAndRotation(record.id, target, orientation, JPH::EActivation::DontActivate);
	}

	m_impl->contacts.beginStep();
	const auto started = std::chrono::steady_clock::now();
	const auto error = m_impl->system->Update(
		fixedDeltaTime, 1, m_impl->allocator.get(), m_impl->jobs.get());
	m_stats.stepMilliseconds = std::chrono::duration<f32, std::milli>(
		std::chrono::steady_clock::now() - started).count();
	if (error != JPH::EPhysicsUpdateError::None) return;

	m_stats.activeBodyCount = 0;
	for (const auto& [entityId, record] : m_impl->bodies)
	{
		if (interface.IsActive(record.id)) ++m_stats.activeBodyCount;
		if (record.motion != RigidBodyType::Dynamic) continue;
		auto* object = world.findGameObject(entityId);
		if (!object) continue;
		JPH::RVec3 position{};
		JPH::Quat rotation{};
		interface.GetPositionAndRotation(record.id, position, rotation);
		auto& transform = object->getTransform();
		transform.setPosition({ static_cast<f32>(position.GetX()),
			static_cast<f32>(position.GetY()), static_cast<f32>(position.GetZ()) });
		transform.setRotationQuaternion(
			{ rotation.GetX(), rotation.GetY(), rotation.GetZ(), rotation.GetW() },
			transform.getRotation());
	}
	m_impl->contacts.snapshot(m_impl->contactEvents, m_impl->activeContacts);
	m_stats.contactCount = static_cast<ui32>(m_impl->activeContacts.size());
	m_stats.contactEventCount = static_cast<ui32>(m_impl->contactEvents.size());
}

bool dx3d::PhysicsWorld::raycast(const Vec3& origin, const Vec3& direction,
	f32 maxDistance, PhysicsRaycastHit& hit) const
{
	if (!m_impl || maxDistance <= 0.0f) return false;
	const f32 lengthSquared = direction.x * direction.x + direction.y * direction.y
		+ direction.z * direction.z;
	if (lengthSquared <= 0.000001f) return false;
	const f32 inverse = maxDistance / std::sqrt(lengthSquared);
	const JPH::RRayCast ray(JPH::RVec3(origin.x, origin.y, origin.z),
		JPH::Vec3(direction.x * inverse, direction.y * inverse, direction.z * inverse));
	JPH::RayCastResult result{};
	if (!m_impl->system->GetNarrowPhaseQuery().CastRay(ray, result)) return false;
	const JPH::BodyLockRead lock(m_impl->system->GetBodyLockInterface(), result.mBodyID);
	if (!lock.Succeeded()) return false;
	const JPH::RVec3 position = ray.GetPointOnRay(result.mFraction);
	const JPH::Vec3 normal = lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2, position);
	hit.entityId = lock.GetBody().GetUserData();
	hit.position = { static_cast<f32>(position.GetX()), static_cast<f32>(position.GetY()), static_cast<f32>(position.GetZ()) };
	hit.normal = { normal.GetX(), normal.GetY(), normal.GetZ() };
	hit.distance = result.mFraction * maxDistance;
	return true;
}

const dx3d::PhysicsWorld::Stats& dx3d::PhysicsWorld::getStats() const noexcept { return m_stats; }
const std::vector<dx3d::PhysicsContactEvent>& dx3d::PhysicsWorld::getContactEvents() const noexcept
{
	static const std::vector<PhysicsContactEvent> empty{};
	return m_impl ? m_impl->contactEvents : empty;
}
const std::vector<dx3d::PhysicsContact>& dx3d::PhysicsWorld::getActiveContacts() const noexcept
{
	static const std::vector<PhysicsContact> empty{};
	return m_impl ? m_impl->activeContacts : empty;
}
