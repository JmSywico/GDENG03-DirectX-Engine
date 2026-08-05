#include <DX3D/Component/ColliderComponent.h>

#include <algorithm>

dx3d::ColliderComponent::ColliderComponent(const ComponentDesc& data) : Component(data) {}
void dx3d::ColliderComponent::setShape(ColliderShape value) noexcept { m_shape = value; }
dx3d::ColliderShape dx3d::ColliderComponent::getShape() const noexcept { return m_shape; }
void dx3d::ColliderComponent::setHalfExtents(const Vec3& value) noexcept
{
	m_halfExtents = {
		std::max(0.001f, value.x),
		std::max(0.001f, value.y),
		std::max(0.001f, value.z)
	};
}
dx3d::Vec3 dx3d::ColliderComponent::getHalfExtents() const noexcept { return m_halfExtents; }
void dx3d::ColliderComponent::setRadius(f32 value) noexcept { m_radius = std::max(0.001f, value); }
dx3d::f32 dx3d::ColliderComponent::getRadius() const noexcept { return m_radius; }
