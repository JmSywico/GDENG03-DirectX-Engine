#include <DX3D/Component/RigidBodyComponent.h>
#include <DX3D/Game/GameObject.h>
#include <DX3D/Component/TransformComponent.h>

#include <algorithm>

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
}

dx3d::Vec3
dx3d::RigidBodyComponent::getVelocity() const noexcept
{
	return m_velocity;
}

void dx3d::RigidBodyComponent::setAngularVelocity(
	const Vec3& angularVelocity
) noexcept
{
	m_angularVelocity = angularVelocity;
}

dx3d::Vec3
dx3d::RigidBodyComponent::getAngularVelocity() const noexcept
{
	return m_angularVelocity;
}

void dx3d::RigidBodyComponent::setMass(
	f32 mass
) noexcept
{
	if (mass <= 0.0f)
		return;

	m_mass = mass;
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
}

bool dx3d::RigidBodyComponent::hasInitialState() const noexcept
{
	return m_hasInitialState;
}