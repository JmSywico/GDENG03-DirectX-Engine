#include <DX3D/Component/RigidBodyComponent.h>

#include <algorithm>

dx3d::RigidBodyComponent::RigidBodyComponent(const ComponentDesc& data) : Component(data) {}
void dx3d::RigidBodyComponent::setBodyType(RigidBodyType value) noexcept { m_bodyType = value; }
dx3d::RigidBodyType dx3d::RigidBodyComponent::getBodyType() const noexcept { return m_bodyType; }
void dx3d::RigidBodyComponent::setColliderShape(ColliderShape value) noexcept { m_colliderShape = value; }
dx3d::ColliderShape dx3d::RigidBodyComponent::getColliderShape() const noexcept { return m_colliderShape; }
void dx3d::RigidBodyComponent::setHalfExtents(const Vec3& value) noexcept
{
	m_halfExtents = { std::max(0.001f, value.x), std::max(0.001f, value.y), std::max(0.001f, value.z) };
}
dx3d::Vec3 dx3d::RigidBodyComponent::getHalfExtents() const noexcept { return m_halfExtents; }
void dx3d::RigidBodyComponent::setRadius(f32 value) noexcept { m_radius = std::max(0.001f, value); }
dx3d::f32 dx3d::RigidBodyComponent::getRadius() const noexcept { return m_radius; }
void dx3d::RigidBodyComponent::setMass(f32 value) noexcept { m_mass = std::max(0.001f, value); }
dx3d::f32 dx3d::RigidBodyComponent::getMass() const noexcept { return m_mass; }
void dx3d::RigidBodyComponent::setRestitution(f32 value) noexcept { m_restitution = std::clamp(value, 0.0f, 1.0f); }
dx3d::f32 dx3d::RigidBodyComponent::getRestitution() const noexcept { return m_restitution; }
void dx3d::RigidBodyComponent::setGravityEnabled(bool value) noexcept { m_gravityEnabled = value; }
bool dx3d::RigidBodyComponent::isGravityEnabled() const noexcept { return m_gravityEnabled; }
void dx3d::RigidBodyComponent::setLinearVelocity(const Vec3& value) noexcept { m_linearVelocity = value; }
dx3d::Vec3 dx3d::RigidBodyComponent::getLinearVelocity() const noexcept { return m_linearVelocity; }
