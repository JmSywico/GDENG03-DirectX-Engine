#pragma once

#include <DX3D/Game/Component.h>
#include <DX3D/Math/Vec3.h>

namespace dx3d
{
	enum class ColliderShape : ui32 { Box = 0, Sphere, Cylinder, Capsule };

	class ColliderComponent final : public Component
	{
		dx3d_typeid(ColliderComponent)
	public:
		explicit ColliderComponent(const ComponentDesc& data);

		void setShape(ColliderShape value) noexcept;
		ColliderShape getShape() const noexcept;
		void setHalfExtents(const Vec3& value) noexcept;
		Vec3 getHalfExtents() const noexcept;
		void setRadius(f32 value) noexcept;
		f32 getRadius() const noexcept;

	private:
		ColliderShape m_shape{ ColliderShape::Box };
		Vec3 m_halfExtents{ 0.5f, 0.5f, 0.5f };
		f32 m_radius{ 0.5f };
	};
}
