#pragma once

#include <DX3D/Game/Component.h>
#include <DX3D/Math/Vec3.h>

namespace dx3d
{
	enum class RigidBodyType : ui32 { Static = 0, Dynamic, Kinematic };
	enum class ColliderShape : ui32 { Box = 0, Sphere };

	class RigidBodyComponent final : public Component
	{
		dx3d_typeid(RigidBodyComponent)
	public:
		explicit RigidBodyComponent(const ComponentDesc& data);

		void setBodyType(RigidBodyType type) noexcept;
		RigidBodyType getBodyType() const noexcept;
		void setColliderShape(ColliderShape shape) noexcept;
		ColliderShape getColliderShape() const noexcept;
		void setHalfExtents(const Vec3& value) noexcept;
		Vec3 getHalfExtents() const noexcept;
		void setRadius(f32 value) noexcept;
		f32 getRadius() const noexcept;
		void setMass(f32 value) noexcept;
		f32 getMass() const noexcept;
		void setRestitution(f32 value) noexcept;
		f32 getRestitution() const noexcept;
		void setGravityEnabled(bool value) noexcept;
		bool isGravityEnabled() const noexcept;
		void setLinearVelocity(const Vec3& value) noexcept;
		Vec3 getLinearVelocity() const noexcept;

	private:
		RigidBodyType m_bodyType{ RigidBodyType::Dynamic };
		ColliderShape m_colliderShape{ ColliderShape::Box };
		Vec3 m_halfExtents{ 0.5f, 0.5f, 0.5f };
		f32 m_radius{ 0.5f };
		f32 m_mass{ 1.0f };
		f32 m_restitution{ 0.1f };
		bool m_gravityEnabled{ true };
		Vec3 m_linearVelocity{};
	};
}
