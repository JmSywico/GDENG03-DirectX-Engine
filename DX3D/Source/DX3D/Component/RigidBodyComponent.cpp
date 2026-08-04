#include <DX3D/Component/RigidBodyComponent.h>
#include <DX3D/Game/GameObject.h>
#include <DX3D/Component/TransformComponent.h>

#include <reactphysics3d/reactphysics3d.h>

#include <algorithm>

namespace
{
	reactphysics3d::Vector3 toRuntimeVector(
		const dx3d::Vec3& value
	) noexcept
	{
		return
		{
			value.x,
			value.y,
			value.z
		};
	}

	dx3d::Vec3 fromRuntimeVector(
		const reactphysics3d::Vector3& value
	) noexcept
	{
		return
		{
			static_cast<dx3d::f32>(value.x),
			static_cast<dx3d::f32>(value.y),
			static_cast<dx3d::f32>(value.z)
		};
	}
}

dx3d::RigidBodyComponent::RigidBodyComponent(
	const ComponentDesc& desc
)
	: Component(desc)
{}

void dx3d::RigidBodyComponent::setVelocity(
	const Vec3& velocity
) noexcept
{
	m_velocity = velocity;

	if (m_runtimeBody)
	{
		m_runtimeBody->setLinearVelocity(
			toRuntimeVector(m_velocity)
		);
	}
}

dx3d::Vec3
dx3d::RigidBodyComponent::getVelocity() const noexcept
{
	if (m_runtimeBody)
	{
		return fromRuntimeVector(
			m_runtimeBody->
			getLinearVelocity()
		);
	}

	return m_velocity;
}

void dx3d::RigidBodyComponent::setAngularVelocity(
	const Vec3& angularVelocity
) noexcept
{
	m_angularVelocity = angularVelocity;

	if (m_runtimeBody)
	{
		m_runtimeBody->setAngularVelocity(
			toRuntimeVector(m_angularVelocity)
		);
	}
}

dx3d::Vec3
dx3d::RigidBodyComponent::getAngularVelocity() const noexcept
{
	if (m_runtimeBody)
	{
		return fromRuntimeVector(
			m_runtimeBody->
			getAngularVelocity()
		);
	}

	return m_angularVelocity;
}

void dx3d::RigidBodyComponent::setMass(
	f32 mass
) noexcept
{
	if (mass <= 0.0f)
		return;

	m_mass = mass;

	if (m_runtimeBody)
	{
		m_runtimeBody->setMass(
			m_mass
		);

		if (m_runtimeShape)
		{
			m_runtimeBody->setLocalInertiaTensor(
				m_runtimeShape->
				getLocalInertiaTensor(m_mass)
			);
		}
	}
}

dx3d::f32
dx3d::RigidBodyComponent::getMass() const noexcept
{
	return m_mass;
}

void dx3d::RigidBodyComponent::setRestitution(
	f32 restitution
) noexcept
{
	m_restitution = std::clamp(
		restitution,
		0.0f,
		1.0f
	);

	if (m_runtimeCollider)
	{
		m_runtimeCollider->
			getMaterial().
			setBounciness(
				m_restitution
			);
	}
}

dx3d::f32
dx3d::RigidBodyComponent::getRestitution() const noexcept
{
	return m_restitution;
}

void dx3d::RigidBodyComponent::setFriction(
	f32 friction
) noexcept
{
	m_friction = std::clamp(
		friction,
		0.0f,
		1.0f
	);

	if (m_runtimeCollider)
	{
		m_runtimeCollider->
			getMaterial().
			setFrictionCoefficient(
				m_friction
			);
	}
}

dx3d::f32
dx3d::RigidBodyComponent::getFriction() const noexcept
{
	return m_friction;
}

void dx3d::RigidBodyComponent::setUseGravity(
	bool useGravity
) noexcept
{
	m_useGravity = useGravity;

	if (m_runtimeBody)
	{
		m_runtimeBody->enableGravity(
			m_useGravity
		);
	}
}

bool dx3d::RigidBodyComponent::getUseGravity() const noexcept
{
	return m_useGravity;
}

void dx3d::RigidBodyComponent::setStatic(
	bool isStatic
) noexcept
{
	m_isStatic = isStatic;

	if (m_runtimeBody)
	{
		m_runtimeBody->setType(
			m_isStatic
			? reactphysics3d::BodyType::STATIC
			: reactphysics3d::BodyType::DYNAMIC
		);

		if (!m_isStatic)
		{
			m_runtimeBody->setMass(
				m_mass
			);

			m_runtimeBody->setLinearVelocity(
				toRuntimeVector(m_velocity)
			);

			m_runtimeBody->setAngularVelocity(
				toRuntimeVector(m_angularVelocity)
			);
		}
	}
}

bool dx3d::RigidBodyComponent::getStatic() const noexcept
{
	return m_isStatic;
}

void dx3d::RigidBodyComponent::captureInitialState()
{
	auto& transform =
		m_object.getTransform();

	m_initialPosition =
		transform.getPosition();

	m_initialRotation =
		transform.getRotation();

	m_initialScale =
		transform.getScale();

	m_initialVelocity =
		m_velocity;

	m_initialAngularVelocity =
		m_angularVelocity;

	m_hasInitialState = true;
}

void dx3d::RigidBodyComponent::restoreInitialState()
{
	if (!m_hasInitialState)
		return;

	auto& transform =
		m_object.getTransform();

	transform.setPosition(
		m_initialPosition
	);

	transform.setRotation(
		m_initialRotation
	);

	transform.setScale(
		m_initialScale
	);

	m_velocity =
		m_initialVelocity;

	m_angularVelocity =
		m_initialAngularVelocity;

	if (m_runtimeBody)
	{
		const auto position =
			m_initialPosition;

		const auto rotation =
			m_initialRotation;

		const reactphysics3d::Transform runtimeTransform
		{
			toRuntimeVector(position),
			reactphysics3d::Quaternion::
			fromEulerAngles(
				rotation.x,
				rotation.y,
				rotation.z
			)
		};

		m_runtimeBody->setTransform(
			runtimeTransform
		);

		m_runtimeBody->setLinearVelocity(
			toRuntimeVector(m_velocity)
		);

		m_runtimeBody->setAngularVelocity(
			toRuntimeVector(m_angularVelocity)
		);
	}
}

bool dx3d::RigidBodyComponent::hasInitialState() const noexcept
{
	return m_hasInitialState;
}

void dx3d::RigidBodyComponent::attachRuntimeBody(
	reactphysics3d::RigidBody* body,
	reactphysics3d::Collider* collider,
	reactphysics3d::BoxShape* shape
) noexcept
{
	m_runtimeBody = body;
	m_runtimeCollider = collider;
	m_runtimeShape = shape;

	applyRuntimeProperties();
}

void dx3d::RigidBodyComponent::detachRuntimeBody() noexcept
{
	m_runtimeBody = nullptr;
	m_runtimeCollider = nullptr;
	m_runtimeShape = nullptr;
}

void dx3d::RigidBodyComponent::syncFromRuntime() noexcept
{
	if (!m_runtimeBody)
		return;

	m_velocity =
		fromRuntimeVector(
			m_runtimeBody->
			getLinearVelocity()
		);

	m_angularVelocity =
		fromRuntimeVector(
			m_runtimeBody->
			getAngularVelocity()
		);
}

void dx3d::RigidBodyComponent::applyRuntimeProperties() noexcept
{
	if (!m_runtimeBody)
		return;

	m_runtimeBody->setType(
		m_isStatic
		? reactphysics3d::BodyType::STATIC
		: reactphysics3d::BodyType::DYNAMIC
	);

	m_runtimeBody->enableGravity(
		m_useGravity
	);

	m_runtimeBody->setMass(
		m_mass
	);

	if (m_runtimeShape)
	{
		m_runtimeBody->setLocalInertiaTensor(
			m_runtimeShape->
			getLocalInertiaTensor(m_mass)
		);
	}

	m_runtimeBody->setLinearVelocity(
		toRuntimeVector(m_velocity)
	);

	m_runtimeBody->setAngularVelocity(
		toRuntimeVector(m_angularVelocity)
	);

	if (m_runtimeCollider)
	{
		auto& material =
			m_runtimeCollider->getMaterial();

		material.setBounciness(
			m_restitution
		);

		material.setFrictionCoefficient(
			m_friction
		);
	}
}
