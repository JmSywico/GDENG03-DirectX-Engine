#include <DX3D/Component/RigidBodyComponent.h>

#include <algorithm>
#include <cmath>

dx3d::RigidBodyComponent::RigidBodyComponent(const ComponentDesc& data) : Component(data) {}
void dx3d::RigidBodyComponent::setBodyType(RigidBodyType value) noexcept { m_bodyType = value; }
dx3d::RigidBodyType dx3d::RigidBodyComponent::getBodyType() const noexcept { return m_bodyType; }
void dx3d::RigidBodyComponent::setFriction(f32 value) noexcept { if (std::isfinite(value)) m_friction = std::max(0.0f, value); }
dx3d::f32 dx3d::RigidBodyComponent::getFriction() const noexcept { return m_friction; }
void dx3d::RigidBodyComponent::setRestitution(f32 value) noexcept { if (std::isfinite(value)) m_restitution = std::clamp(value, 0.0f, 1.0f); }
dx3d::f32 dx3d::RigidBodyComponent::getRestitution() const noexcept { return m_restitution; }
void dx3d::RigidBodyComponent::setLinearDamping(f32 value) noexcept { if (std::isfinite(value)) m_linearDamping = std::max(0.0f, value); }
dx3d::f32 dx3d::RigidBodyComponent::getLinearDamping() const noexcept { return m_linearDamping; }
void dx3d::RigidBodyComponent::setAngularDamping(f32 value) noexcept { if (std::isfinite(value)) m_angularDamping = std::max(0.0f, value); }
dx3d::f32 dx3d::RigidBodyComponent::getAngularDamping() const noexcept { return m_angularDamping; }
void dx3d::RigidBodyComponent::setGravityFactor(f32 value) noexcept { if (std::isfinite(value)) m_gravityFactor = value; }
dx3d::f32 dx3d::RigidBodyComponent::getGravityFactor() const noexcept { return m_gravityFactor; }
void dx3d::RigidBodyComponent::setEnabled(bool value) noexcept { m_enabled = value; }
bool dx3d::RigidBodyComponent::isEnabled() const noexcept { return m_enabled; }
