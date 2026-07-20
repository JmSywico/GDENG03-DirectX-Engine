#include <DX3D/Physics/PhysicsSystem.h>

#include <DX3D/Game/World.h>
#include <DX3D/Game/GameObject.h>

#include <DX3D/Component/RigidBodyComponent.h>
#include <DX3D/Component/TransformComponent.h>
#include <DX3D/Component/CubeComponent.h>
#include <DX3D/Component/PlaneComponent.h>

#include <algorithm>
#include <cmath>

namespace dx3d
{
	namespace
	{
		constexpr f32 Gravity = -9.81f;
		constexpr f32 RestingVelocity = 0.15f;

		constexpr f32 LinearDamping = 0.999f;
		constexpr f32 AngularDamping = 0.995f;

		constexpr f32 SphereRadiusScale = 0.52f;
		constexpr f32 CollisionSlop = 0.01f;
		constexpr f32 PositionCorrectionPercent = 0.65f;

		constexpr ui32 CollisionIterations = 6;

		bool isCubeRigidBody(
			RigidBodyComponent* rigidBody
		)
		{
			if (!rigidBody)
				return false;

			GameObject& object =
				rigidBody->getGameObject();

			return object.getComponent<
				CubeComponent>() != nullptr;
		}

		void integrateRigidBody(
			RigidBodyComponent& rigidBody,
			f32 fixedDeltaTime
		)
		{
			if (rigidBody.getStatic())
				return;

			GameObject& object =
				rigidBody.getGameObject();

			if (!object.getComponent<
				CubeComponent>())
			{
				return;
			}

			TransformComponent& transform =
				object.getTransform();

			Vec3 position =
				transform.getPosition();

			Vec3 rotation =
				transform.getRotation();

			Vec3 velocity =
				rigidBody.getVelocity();

			Vec3 angularVelocity =
				rigidBody.getAngularVelocity();

			if (rigidBody.getUseGravity())
			{
				velocity.y +=
					Gravity *
					fixedDeltaTime;
			}

			velocity *=
				LinearDamping;

			angularVelocity *=
				AngularDamping;

			position +=
				velocity *
				fixedDeltaTime;

			rotation +=
				angularVelocity *
				fixedDeltaTime;

			rigidBody.setVelocity(
				velocity
			);

			rigidBody.setAngularVelocity(
				angularVelocity
			);

			transform.setPosition(
				position
			);

			transform.setRotation(
				rotation
			);
		}

		void resolvePlaneCollision(
			RigidBodyComponent& rigidBody,
			PlaneComponent* const* planes,
			ui32 planeCount
		)
		{
			if (rigidBody.getStatic())
				return;

			GameObject& object =
				rigidBody.getGameObject();

			if (!object.getComponent<
				CubeComponent>())
			{
				return;
			}

			TransformComponent& transform =
				object.getTransform();

			Vec3 position =
				transform.getPosition();

			const Vec3 scale =
				transform.getScale();

			Vec3 velocity =
				rigidBody.getVelocity();

			Vec3 angularVelocity =
				rigidBody.getAngularVelocity();

			const f32 cubeHalfWidth =
				std::fabs(scale.x) *
				0.5f;

			const f32 cubeHalfHeight =
				std::fabs(scale.y) *
				0.5f;

			const f32 cubeHalfDepth =
				std::fabs(scale.z) *
				0.5f;

			for (ui32 planeIndex = 0;
				planeIndex < planeCount;
				++planeIndex)
			{
				PlaneComponent* plane =
					planes[planeIndex];

				if (!plane)
					continue;

				TransformComponent& planeTransform =
					plane->
					getGameObject().
					getTransform();

				const Vec3 planePosition =
					planeTransform.getPosition();

				const Vec3 planeScale =
					planeTransform.getScale();

				const f32 planeHalfWidth =
					std::fabs(
						planeScale.x
					) * 0.5f;

				const f32 planeHalfDepth =
					std::fabs(
						planeScale.z
					) * 0.5f;

				const bool overlapsPlaneX =
					position.x + cubeHalfWidth >=
					planePosition.x - planeHalfWidth &&
					position.x - cubeHalfWidth <=
					planePosition.x + planeHalfWidth;

				const bool overlapsPlaneZ =
					position.z + cubeHalfDepth >=
					planePosition.z - planeHalfDepth &&
					position.z - cubeHalfDepth <=
					planePosition.z + planeHalfDepth;

				if (!overlapsPlaneX ||
					!overlapsPlaneZ)
				{
					continue;
				}

				const f32 planeSurfaceY =
					planePosition.y;

				const f32 cubeBottom =
					position.y -
					cubeHalfHeight;

				if (cubeBottom >
					planeSurfaceY)
				{
					continue;
				}

				position.y =
					planeSurfaceY +
					cubeHalfHeight;

				if (velocity.y < 0.0f)
				{
					velocity.y =
						-velocity.y *
						rigidBody.
						getRestitution();

					const f32 frictionMultiplier =
						std::clamp(
							1.0f -
							rigidBody.
							getFriction(),
							0.0f,
							1.0f
						);

					velocity.x *=
						frictionMultiplier;

					velocity.z *=
						frictionMultiplier;

					const f32 angularFriction =
						std::clamp(
							1.0f -
							rigidBody.
							getFriction() *
							0.5f,
							0.0f,
							1.0f
						);

					angularVelocity *=
						angularFriction;

					if (std::fabs(
						velocity.y
					) < RestingVelocity)
					{
						velocity.y = 0.0f;
					}
				}

				transform.setPosition(
					position
				);

				rigidBody.setVelocity(
					velocity
				);

				rigidBody.setAngularVelocity(
					angularVelocity
				);

				break;
			}
		}

		bool resolveCubeCollision(
			RigidBodyComponent& bodyA,
			RigidBodyComponent& bodyB
		)
		{
			const bool bodyAStatic =
				bodyA.getStatic();

			const bool bodyBStatic =
				bodyB.getStatic();

			if (bodyAStatic &&
				bodyBStatic)
			{
				return false;
			}

			GameObject& objectA =
				bodyA.getGameObject();

			GameObject& objectB =
				bodyB.getGameObject();

			if (!objectA.getComponent<
				CubeComponent>() ||
				!objectB.getComponent<
				CubeComponent>())
			{
				return false;
			}

			TransformComponent& transformA =
				objectA.getTransform();

			TransformComponent& transformB =
				objectB.getTransform();

			Vec3 positionA =
				transformA.getPosition();

			Vec3 positionB =
				transformB.getPosition();

			const Vec3 scaleA =
				transformA.getScale();

			const Vec3 scaleB =
				transformB.getScale();

			const f32 maximumScaleA =
				std::max(
					std::fabs(scaleA.x),
					std::max(
						std::fabs(scaleA.y),
						std::fabs(scaleA.z)
					)
				);

			const f32 maximumScaleB =
				std::max(
					std::fabs(scaleB.x),
					std::max(
						std::fabs(scaleB.y),
						std::fabs(scaleB.z)
					)
				);

			const f32 radiusA =
				maximumScaleA *
				SphereRadiusScale;

			const f32 radiusB =
				maximumScaleB *
				SphereRadiusScale;

			const f32 differenceX =
				positionB.x -
				positionA.x;

			const f32 differenceY =
				positionB.y -
				positionA.y;

			const f32 differenceZ =
				positionB.z -
				positionA.z;

			const f32 distanceSquared =
				differenceX * differenceX +
				differenceY * differenceY +
				differenceZ * differenceZ;

			const f32 combinedRadius =
				radiusA +
				radiusB;

			if (distanceSquared >=
				combinedRadius *
				combinedRadius)
			{
				return false;
			}

			Vec3 velocityA =
				bodyA.getVelocity();

			Vec3 velocityB =
				bodyB.getVelocity();

			Vec3 collisionNormal{};

			f32 distance = 0.0f;

			if (distanceSquared >
				0.000001f)
			{
				distance =
					std::sqrt(
						distanceSquared
					);

				const f32 inverseDistance =
					1.0f /
					distance;

				collisionNormal =
				{
					differenceX *
						inverseDistance,

					differenceY *
						inverseDistance,

					differenceZ *
						inverseDistance
				};
			}
			else
			{
				const f32 relativeX =
					velocityB.x -
					velocityA.x;

				const f32 relativeY =
					velocityB.y -
					velocityA.y;

				const f32 relativeZ =
					velocityB.z -
					velocityA.z;

				const f32 relativeLengthSquared =
					relativeX * relativeX +
					relativeY * relativeY +
					relativeZ * relativeZ;

				if (relativeLengthSquared >
					0.000001f)
				{
					const f32 relativeLength =
						std::sqrt(
							relativeLengthSquared
						);

					collisionNormal =
					{
						relativeX /
							relativeLength,

						relativeY /
							relativeLength,

						relativeZ /
							relativeLength
					};
				}
				else
				{
					collisionNormal =
					{
						1.0f,
						0.0f,
						0.0f
					};
				}
			}

			const f32 penetrationDepth =
				combinedRadius -
				distance;

			const f32 inverseMassA =
				bodyAStatic
				? 0.0f
				: 1.0f /
				std::max(
					bodyA.getMass(),
					0.001f
				);

			const f32 inverseMassB =
				bodyBStatic
				? 0.0f
				: 1.0f /
				std::max(
					bodyB.getMass(),
					0.001f
				);

			const f32 totalInverseMass =
				inverseMassA +
				inverseMassB;

			if (totalInverseMass <= 0.0f)
				return false;

			const f32 correctionMagnitude =
				std::max(
					penetrationDepth -
					CollisionSlop,
					0.0f
				) *
				PositionCorrectionPercent /
				totalInverseMass;

			positionA.x -=
				collisionNormal.x *
				correctionMagnitude *
				inverseMassA;

			positionA.y -=
				collisionNormal.y *
				correctionMagnitude *
				inverseMassA;

			positionA.z -=
				collisionNormal.z *
				correctionMagnitude *
				inverseMassA;

			positionB.x +=
				collisionNormal.x *
				correctionMagnitude *
				inverseMassB;

			positionB.y +=
				collisionNormal.y *
				correctionMagnitude *
				inverseMassB;

			positionB.z +=
				collisionNormal.z *
				correctionMagnitude *
				inverseMassB;

			const f32 relativeVelocityX =
				velocityB.x -
				velocityA.x;

			const f32 relativeVelocityY =
				velocityB.y -
				velocityA.y;

			const f32 relativeVelocityZ =
				velocityB.z -
				velocityA.z;

			const f32 normalVelocity =
				relativeVelocityX *
				collisionNormal.x +
				relativeVelocityY *
				collisionNormal.y +
				relativeVelocityZ *
				collisionNormal.z;

			f32 normalImpulseMagnitude =
				0.0f;

			if (normalVelocity < 0.0f)
			{
				const f32 restitution =
					std::min(
						0.75f,
						std::min(
							bodyA.getRestitution(),
							bodyB.getRestitution()
						)
					);

				normalImpulseMagnitude =
					-(1.0f + restitution) *
					normalVelocity /
					totalInverseMass;

				const Vec3 normalImpulse
				{
					collisionNormal.x *
						normalImpulseMagnitude,

					collisionNormal.y *
						normalImpulseMagnitude,

					collisionNormal.z *
						normalImpulseMagnitude
				};

				velocityA.x -=
					normalImpulse.x *
					inverseMassA;

				velocityA.y -=
					normalImpulse.y *
					inverseMassA;

				velocityA.z -=
					normalImpulse.z *
					inverseMassA;

				velocityB.x +=
					normalImpulse.x *
					inverseMassB;

				velocityB.y +=
					normalImpulse.y *
					inverseMassB;

				velocityB.z +=
					normalImpulse.z *
					inverseMassB;

				const f32 tangentVelocityX =
					relativeVelocityX -
					collisionNormal.x *
					normalVelocity;

				const f32 tangentVelocityY =
					relativeVelocityY -
					collisionNormal.y *
					normalVelocity;

				const f32 tangentVelocityZ =
					relativeVelocityZ -
					collisionNormal.z *
					normalVelocity;

				const f32 tangentLengthSquared =
					tangentVelocityX *
					tangentVelocityX +
					tangentVelocityY *
					tangentVelocityY +
					tangentVelocityZ *
					tangentVelocityZ;

				if (tangentLengthSquared >
					0.000001f)
				{
					const f32 tangentLength =
						std::sqrt(
							tangentLengthSquared
						);

					const Vec3 tangent
					{
						tangentVelocityX /
							tangentLength,

						tangentVelocityY /
							tangentLength,

						tangentVelocityZ /
							tangentLength
					};

					f32 frictionImpulseMagnitude =
						-(
							relativeVelocityX *
							tangent.x +
							relativeVelocityY *
							tangent.y +
							relativeVelocityZ *
							tangent.z
							) /
						totalInverseMass;

					const f32 frictionCoefficient =
						std::sqrt(
							std::max(
								bodyA.
								getFriction() *
								bodyB.
								getFriction(),
								0.0f
							)
						);

					const f32 maximumFrictionImpulse =
						normalImpulseMagnitude *
						frictionCoefficient;

					frictionImpulseMagnitude =
						std::clamp(
							frictionImpulseMagnitude,
							-maximumFrictionImpulse,
							maximumFrictionImpulse
						);

					const Vec3 frictionImpulse
					{
						tangent.x *
							frictionImpulseMagnitude,

						tangent.y *
							frictionImpulseMagnitude,

						tangent.z *
							frictionImpulseMagnitude
					};

					velocityA.x -=
						frictionImpulse.x *
						inverseMassA;

					velocityA.y -=
						frictionImpulse.y *
						inverseMassA;

					velocityA.z -=
						frictionImpulse.z *
						inverseMassA;

					velocityB.x +=
						frictionImpulse.x *
						inverseMassB;

					velocityB.y +=
						frictionImpulse.y *
						inverseMassB;

					velocityB.z +=
						frictionImpulse.z *
						inverseMassB;
				}
			}

			Vec3 angularVelocityA =
				bodyA.getAngularVelocity();

			Vec3 angularVelocityB =
				bodyB.getAngularVelocity();

			angularVelocityA *=
				0.985f;

			angularVelocityB *=
				0.985f;

			transformA.setPosition(
				positionA
			);

			transformB.setPosition(
				positionB
			);

			bodyA.setVelocity(
				velocityA
			);

			bodyB.setVelocity(
				velocityB
			);

			bodyA.setAngularVelocity(
				angularVelocityA
			);

			bodyB.setAngularVelocity(
				angularVelocityB
			);

			return true;
		}
	}
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
	ui32 rigidBodyCount = 0;

	RigidBodyComponent* const* rigidBodies =
		world.getComponents<
		RigidBodyComponent>(
			rigidBodyCount
		);

	if (!rigidBodies ||
		rigidBodyCount == 0)
	{
		return;
	}

	ui32 planeCount = 0;

	PlaneComponent* const* planes =
		world.getComponents<
		PlaneComponent>(
			planeCount
		);

	for (ui32 rigidBodyIndex = 0;
		rigidBodyIndex < rigidBodyCount;
		++rigidBodyIndex)
	{
		RigidBodyComponent* rigidBody =
			rigidBodies[rigidBodyIndex];

		if (!isCubeRigidBody(
			rigidBody
		))
		{
			continue;
		}

		integrateRigidBody(
			*rigidBody,
			fixedDeltaTime
		);
	}

	for (ui32 rigidBodyIndex = 0;
		rigidBodyIndex < rigidBodyCount;
		++rigidBodyIndex)
	{
		RigidBodyComponent* rigidBody =
			rigidBodies[rigidBodyIndex];

		if (!isCubeRigidBody(
			rigidBody
		))
		{
			continue;
		}

		resolvePlaneCollision(
			*rigidBody,
			planes,
			planeCount
		);
	}

	for (ui32 iteration = 0;
		iteration < CollisionIterations;
		++iteration)
	{
		bool collisionFound = false;

		for (ui32 firstIndex = 0;
			firstIndex < rigidBodyCount;
			++firstIndex)
		{
			RigidBodyComponent* firstBody =
				rigidBodies[firstIndex];

			if (!isCubeRigidBody(
				firstBody
			))
			{
				continue;
			}

			for (ui32 secondIndex =
				firstIndex + 1;
				secondIndex < rigidBodyCount;
				++secondIndex)
			{
				RigidBodyComponent* secondBody =
					rigidBodies[secondIndex];

				if (!isCubeRigidBody(
					secondBody
				))
				{
					continue;
				}

				if (resolveCubeCollision(
					*firstBody,
					*secondBody
				))
				{
					collisionFound = true;
				}
			}
		}

		if (!collisionFound)
			break;
	}

	for (ui32 rigidBodyIndex = 0;
		rigidBodyIndex < rigidBodyCount;
		++rigidBodyIndex)
	{
		RigidBodyComponent* rigidBody =
			rigidBodies[rigidBodyIndex];

		if (!isCubeRigidBody(
			rigidBody
		))
		{
			continue;
		}

		resolvePlaneCollision(
			*rigidBody,
			planes,
			planeCount
		);
	}
}

void dx3d::PhysicsSystem::resetAccumulator() noexcept
{
	m_accumulator = 0.0f;
}