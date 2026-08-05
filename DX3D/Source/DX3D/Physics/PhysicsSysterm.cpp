#include <DX3D/Physics/PhysicsSystem.h>

#include <DX3D/Game/World.h>
#include <DX3D/Game/GameObject.h>

#include <DX3D/Component/RigidBodyComponent.h>
#include <DX3D/Component/TransformComponent.h>
#include <DX3D/Component/CubeComponent.h>
#include <DX3D/Component/PlaneComponent.h>
#include <DX3D/Component/SphereComponent.h>
#include <DX3D/Component/CapsuleComponent.h>
#include <DX3D/Math/MathUtils.h>

#include <reactphysics3d/reactphysics3d.h>

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace dx3d
{
	namespace
	{
		constexpr f32 Gravity = -9.81f;
		constexpr f32 MinimumHalfExtent = 0.001f;
		constexpr f32 PlaneHalfThickness = 0.05f;
		constexpr f32 PlaneFriction = 0.70f;
		constexpr f32 PlaneRestitution = 0.0f;

		reactphysics3d::Vector3 toRuntimeVector(
			const Vec3& value
		) noexcept
		{
			return
			{
				value.x,
				value.y,
				value.z
			};
		}

		Vec3 fromRuntimeVector(
			const reactphysics3d::Vector3& value
		) noexcept
		{
			return
			{
				static_cast<f32>(value.x),
				static_cast<f32>(value.y),
				static_cast<f32>(value.z)
			};
		}

		reactphysics3d::Quaternion toRuntimeQuaternion(
			const Vec3& eulerRotation
		) noexcept
		{
			return reactphysics3d::Quaternion::
				fromEulerAngles(
					eulerRotation.x,
					eulerRotation.y,
					eulerRotation.z
				);
		}

		reactphysics3d::Transform toRuntimeTransform(
			TransformComponent& transform
		) noexcept
		{
			return
			{
				toRuntimeVector(
					transform.getPosition()
				),
				toRuntimeQuaternion(
					transform.getRotation()
				)
			};
		}

		Vec3 fromRuntimeQuaternion(
			const reactphysics3d::Quaternion& quaternion
		) noexcept
		{
			const auto matrix =
				quaternion.getMatrix();

			const f32 e02 =
				static_cast<f32>(
					matrix[2][0]
					);

			const f32 e12 =
				static_cast<f32>(
					matrix[2][1]
					);

			const f32 e22 =
				static_cast<f32>(
					matrix[2][2]
					);

			const f32 e01 =
				static_cast<f32>(
					matrix[1][0]
					);

			const f32 e00 =
				static_cast<f32>(
					matrix[0][0]
					);

			const f32 e10 =
				static_cast<f32>(
					matrix[0][1]
					);

			const f32 e11 =
				static_cast<f32>(
					matrix[1][1]
					);

			Vec3 euler{};

			const f32 sinY =
				std::clamp(
					-e02,
					-1.0f,
					1.0f
				);

			euler.y =
				std::asin(sinY);

			const f32 cosY =
				std::cos(euler.y);

			if (std::fabs(cosY) >
				0.00001f)
			{
				euler.x =
					std::atan2(
						e12,
						e22
					);

				euler.z =
					std::atan2(
						e01,
						e00
					);
			}
			else
			{
				euler.z = 0.0f;

				if (e02 < 0.0f)
				{
					euler.y =
						MathUtils::PI *
						0.5f;

					euler.x =
						std::atan2(
							e10,
							e11
						);
				}
				else
				{
					euler.y =
						-MathUtils::PI *
						0.5f;

					euler.x =
						std::atan2(
							-e10,
							e11
						);
				}
			}

			return euler;
		}

		reactphysics3d::Vector3 getRigidBodyHalfExtents(
			const RigidBodyComponent& rigidBody
		) noexcept
		{
			const Vec3 colliderSize =
				rigidBody.
				getEffectiveColliderSize();

			return
			{
				std::max(
					std::fabs(colliderSize.x) * 0.5f,
					MinimumHalfExtent
				),
				std::max(
					std::fabs(colliderSize.y) * 0.5f,
					MinimumHalfExtent
				),
				std::max(
					std::fabs(colliderSize.z) * 0.5f,
					MinimumHalfExtent
				)
			};
		}

		reactphysics3d::Transform getColliderTransform(
			const RigidBodyComponent& rigidBody
		) noexcept
		{
			return
			{
				toRuntimeVector(
					rigidBody.
					getColliderOffset()
				),
				reactphysics3d::Quaternion::
				identity()
			};
		}

		reactphysics3d::Vector3 getPlaneHalfExtents(
			TransformComponent& transform
		) noexcept
		{
			const Vec3 scale =
				transform.getScale();

			return
			{
				std::max(
					std::fabs(scale.x) * 0.5f,
					MinimumHalfExtent
				),
				PlaneHalfThickness,
				std::max(
					std::fabs(scale.z) * 0.5f,
					MinimumHalfExtent
				)
			};
		}

		bool areHalfExtentsEqual(
			const Vec3& lhs,
			const reactphysics3d::Vector3& rhs
		) noexcept
		{
			return
				std::fabs(lhs.x - rhs.x) <=
				0.0001f &&
				std::fabs(lhs.y - rhs.y) <=
				0.0001f &&
				std::fabs(lhs.z - rhs.z) <=
				0.0001f;
		}

		bool areVec3Equal(
			const Vec3& lhs,
			const Vec3& rhs
		) noexcept
		{
			return
				std::fabs(lhs.x - rhs.x) <=
				0.0001f &&
				std::fabs(lhs.y - rhs.y) <=
				0.0001f &&
				std::fabs(lhs.z - rhs.z) <=
				0.0001f;
		}

		bool isPrimitiveRigidBody(
			RigidBodyComponent* rigidBody
		) noexcept
		{
			if (!rigidBody)
				return false;

			auto& object =
				rigidBody->getGameObject();

			return
				object.getComponent<
				CubeComponent>() !=
				nullptr ||
				object.getComponent<
				SphereComponent>() !=
				nullptr ||
				object.getComponent<
				CapsuleComponent>() !=
				nullptr;
		}
	}

	struct RigidBodyRuntimeAccess
	{
		static void attach(
			RigidBodyComponent& component,
			reactphysics3d::RigidBody* body,
			reactphysics3d::Collider* collider,
			reactphysics3d::BoxShape* shape
		) noexcept
		{
			component.attachRuntimeBody(
				body,
				collider,
				shape
			);
		}

		static void detach(
			RigidBodyComponent& component
		) noexcept
		{
			component.detachRuntimeBody();
		}

		static void applyRuntimeProperties(
			RigidBodyComponent& component
		) noexcept
		{
			component.applyRuntimeProperties();
		}

		static void syncFromRuntime(
			RigidBodyComponent& component
		) noexcept
		{
			component.syncFromRuntime();
		}
	};

	struct PhysicsSystem::Impl final
	{
		struct RuntimeBody
		{
			GameObject* object{};
			RigidBodyComponent* rigidBody{};
			PlaneComponent* plane{};

			reactphysics3d::RigidBody* body{};
			reactphysics3d::Collider* collider{};
			reactphysics3d::BoxShape* shape{};

			Vec3 halfExtents{};
			Vec3 colliderOffset{};
		};

		~Impl()
		{
			clear(false);
		}

		void ensureWorld()
		{
			if (physicsWorld)
				return;

			reactphysics3d::PhysicsWorld::
				WorldSettings settings{};

			settings.worldName = "DX3D";
			settings.gravity =
				reactphysics3d::Vector3(
					0.0f,
					Gravity,
					0.0f
				);

			physicsWorld =
				physicsCommon.
				createPhysicsWorld(
					settings
				);

			if (physicsWorld)
			{
				physicsWorld->
					setContactsPositionCorrectionTechnique(
						reactphysics3d::
						ContactsPositionCorrectionTechnique::
						SPLIT_IMPULSES
					);
			}
		}

		void clear(
			bool detachComponents
		) noexcept
		{
			for (auto& [component, runtime] :
				rigidBodies)
			{
				destroyRuntimeBody(
					runtime,
					detachComponents
				);
			}

			rigidBodies.clear();

			for (auto& [component, runtime] :
				planes)
			{
				destroyRuntimeBody(
					runtime,
					detachComponents
				);
			}

			planes.clear();

			if (physicsWorld)
			{
				physicsCommon.
					destroyPhysicsWorld(
						physicsWorld
					);

				physicsWorld = nullptr;
			}
		}

		void destroyRuntimeBody(
			RuntimeBody& runtime,
			bool detachComponent
		) noexcept
		{
			if (detachComponent &&
				runtime.rigidBody)
			{
				RigidBodyRuntimeAccess::
					detach(
						*runtime.rigidBody
					);
			}

			if (runtime.body &&
				physicsWorld)
			{
				physicsWorld->
					destroyRigidBody(
						runtime.body
					);
			}

			if (runtime.shape)
			{
				physicsCommon.
					destroyBoxShape(
						runtime.shape
					);
			}

			runtime = RuntimeBody{};
		}

		void removeGameObject(
			GameObject& object
		) noexcept
		{
			if (auto* rigidBody =
				object.getComponent<
				RigidBodyComponent>())
			{
				auto runtimeIt =
					rigidBodies.find(
						rigidBody
					);

				if (runtimeIt !=
					rigidBodies.end())
				{
					destroyRuntimeBody(
						runtimeIt->second,
						true
					);

					rigidBodies.erase(
						runtimeIt
					);
				}
			}

			if (auto* plane =
				object.getComponent<
				PlaneComponent>())
			{
				auto runtimeIt =
					planes.find(
						plane
					);

				if (runtimeIt !=
					planes.end())
				{
					destroyRuntimeBody(
						runtimeIt->second,
						true
					);

					planes.erase(
						runtimeIt
					);
				}
			}
		}

		void syncPlanes(
			World& world
		)
		{
			ui32 planeCount = 0;

			PlaneComponent* const* planeComponents =
				world.getComponents<
				PlaneComponent>(
					planeCount
				);

			for (ui32 index = 0;
				index < planeCount;
				++index)
			{
				PlaneComponent* plane =
					planeComponents[index];

				if (!plane)
					continue;

				auto& transform =
					plane->
					getGameObject().
					getTransform();

				const auto halfExtents =
					getPlaneHalfExtents(
						transform
					);

				auto runtimeIt =
					planes.find(plane);

				if (runtimeIt ==
					planes.end())
				{
					RuntimeBody runtime{};

					runtime.object =
						&plane->getGameObject();

					runtime.plane = plane;

					runtime.shape =
						physicsCommon.
						createBoxShape(
							halfExtents
						);

					runtime.body =
						physicsWorld->
						createRigidBody(
							toRuntimeTransform(
								transform
							)
						);

					runtime.body->setType(
						reactphysics3d::
						BodyType::STATIC
					);

					const reactphysics3d::
						Transform colliderTransform
					{
						{
							0.0f,
							-PlaneHalfThickness,
							0.0f
						},
						reactphysics3d::
						Quaternion::identity()
					};

					runtime.collider =
						runtime.body->
						addCollider(
							runtime.shape,
							colliderTransform
						);

					auto& material =
						runtime.collider->
						getMaterial();

					material.setBounciness(
						PlaneRestitution
					);

					material.
						setFrictionCoefficient(
							PlaneFriction
						);

					runtime.halfExtents =
						fromRuntimeVector(
							halfExtents
						);

					runtimeIt =
						planes.emplace(
							plane,
							runtime
						).first;
				}
				else if (
					!areHalfExtentsEqual(
						runtimeIt->second.
						halfExtents,
						halfExtents
					))
				{
					runtimeIt->second.
						shape->
						setHalfExtents(
							halfExtents
						);

					runtimeIt->second.
						halfExtents =
						fromRuntimeVector(
							halfExtents
						);
				}

				runtimeIt->second.body->
					setTransform(
						toRuntimeTransform(
							transform
						)
					);
			}
		}

		void syncRigidBodies(
			World& world,
			bool syncEngineTransform
		)
		{
			ui32 rigidBodyCount = 0;

			RigidBodyComponent* const* components =
				world.getComponents<
				RigidBodyComponent>(
					rigidBodyCount
				);

			for (ui32 index = 0;
				index < rigidBodyCount;
				++index)
			{
				RigidBodyComponent* rigidBody =
					components[index];

				if (!isPrimitiveRigidBody(
					rigidBody
				))
				{
					continue;
				}

				auto& object =
					rigidBody->
					getGameObject();

				auto& transform =
					object.getTransform();

				const auto halfExtents =
					getRigidBodyHalfExtents(
						*rigidBody
					);

				const Vec3 colliderOffset =
					rigidBody->
					getColliderOffset();

				auto runtimeIt =
					rigidBodies.find(
						rigidBody
					);

				bool shouldSyncEngineTransform =
					syncEngineTransform;

				if (runtimeIt ==
					rigidBodies.end())
				{
					RuntimeBody runtime{};

					runtime.object = &object;
					runtime.rigidBody =
						rigidBody;

					runtime.shape =
						physicsCommon.
						createBoxShape(
							halfExtents
						);

					runtime.body =
						physicsWorld->
						createRigidBody(
							toRuntimeTransform(
								transform
							)
						);

					runtime.body->setUserData(
						&object
					);

					runtime.collider =
						runtime.body->
						addCollider(
							runtime.shape,
							getColliderTransform(
								*rigidBody
							)
						);

					runtime.collider->
						setUserData(
							rigidBody
						);

					runtime.halfExtents =
						fromRuntimeVector(
							halfExtents
						);

					runtime.colliderOffset =
						colliderOffset;

					RigidBodyRuntimeAccess::
						attach(
							*rigidBody,
							runtime.body,
							runtime.collider,
							runtime.shape
						);

					runtimeIt =
						rigidBodies.emplace(
							rigidBody,
							runtime
						).first;

					shouldSyncEngineTransform =
						true;
				}
				else if (
					!areHalfExtentsEqual(
						runtimeIt->second.
						halfExtents,
						halfExtents
					))
				{
					runtimeIt->second.
						shape->
						setHalfExtents(
							halfExtents
						);

					runtimeIt->second.
						halfExtents =
						fromRuntimeVector(
							halfExtents
						);

					if (!rigidBody->getStatic())
					{
						runtimeIt->second.
							body->
							setLocalInertiaTensor(
								runtimeIt->second.
								shape->
								getLocalInertiaTensor(
									rigidBody->
									getMass()
								)
							);
					}
				}

				if (
					runtimeIt->second.
					collider &&
					!areVec3Equal(
						runtimeIt->second.
						colliderOffset,
						colliderOffset
					)
					)
				{
					runtimeIt->second.
						collider->
						setLocalToBodyTransform(
							getColliderTransform(
								*rigidBody
							)
						);

					runtimeIt->second.
						colliderOffset =
						colliderOffset;
				}

				if (shouldSyncEngineTransform ||
					rigidBody->getStatic())
				{
					runtimeIt->second.
						body->
						setTransform(
							toRuntimeTransform(
								transform
							)
						);
				}

				if (shouldSyncEngineTransform)
				{
					RigidBodyRuntimeAccess::
						applyRuntimeProperties(
							*rigidBody
						);
				}
			}
		}

		void syncDynamicRigidBodiesToEngine()
		{
			for (auto& [component, runtime] :
				rigidBodies)
			{
				RigidBodyComponent* rigidBody =
					runtime.rigidBody;

				if (!rigidBody ||
					!runtime.body ||
					rigidBody->getStatic())
				{
					continue;
				}

				const auto& runtimeTransform =
					runtime.body->
					getTransform();

				auto& transform =
					rigidBody->
					getGameObject().
					getTransform();

				transform.setPosition(
					fromRuntimeVector(
						runtimeTransform.
						getPosition()
					)
				);

				transform.setRotation(
					fromRuntimeQuaternion(
						runtimeTransform.
						getOrientation()
					)
				);

				RigidBodyRuntimeAccess::
					syncFromRuntime(
						*rigidBody
					);
			}
		}

		reactphysics3d::PhysicsCommon physicsCommon{};
		reactphysics3d::PhysicsWorld* physicsWorld{};

		std::unordered_map<
			RigidBodyComponent*,
			RuntimeBody
		> rigidBodies{};

		std::unordered_map<
			PlaneComponent*,
			RuntimeBody
		> planes{};
	};
}

dx3d::PhysicsSystem::PhysicsSystem()
	: m_impl(std::make_unique<Impl>())
{}

dx3d::PhysicsSystem::~PhysicsSystem() = default;

void dx3d::PhysicsSystem::start(
	World& world
)
{
	m_impl->ensureWorld();
	m_impl->syncPlanes(world);
	m_impl->syncRigidBodies(
		world,
		true
	);
}

void dx3d::PhysicsSystem::update(
	World& world,
	f32 deltaTime
)
{
	if (deltaTime <= 0.0f)
		return;

	const f32 limitedDeltaTime =
		std::min(
			deltaTime,
			0.25f
		);

	m_accumulator +=
		limitedDeltaTime;

	while (m_accumulator >=
		FixedDeltaTime)
	{
		fixedUpdate(
			world,
			FixedDeltaTime
		);

		m_accumulator -=
			FixedDeltaTime;
	}
}

void dx3d::PhysicsSystem::fixedUpdate(
	World& world,
	f32 fixedDeltaTime
)
{
	m_impl->ensureWorld();
	m_impl->syncPlanes(world);
	m_impl->syncRigidBodies(
		world,
		false
	);

	if (!m_impl->physicsWorld)
		return;

	m_impl->physicsWorld->update(
		fixedDeltaTime
	);

	m_impl->syncDynamicRigidBodiesToEngine();
}

void dx3d::PhysicsSystem::stop(
	World& world
) noexcept
{
	(void)world;
	m_impl->clear(true);
	m_accumulator = 0.0f;
}

void dx3d::PhysicsSystem::removeGameObject(
	GameObject& object
) noexcept
{
	m_impl->removeGameObject(
		object
	);
}

void dx3d::PhysicsSystem::resetAccumulator() noexcept
{
	m_accumulator = 0.0f;
}
