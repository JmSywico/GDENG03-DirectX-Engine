#pragma once

#include <DX3D/Core/Core.h>
#include <DX3D/Game/Component.h>
#include <DX3D/Math/Vec3.h>

namespace dx3d
{
	class RigidBodyComponent final : public Component
	{
		dx3d_typeid(RigidBodyComponent)

	public:

		void captureInitialState();
		void restoreInitialState();

		bool hasInitialState() const noexcept;
		explicit RigidBodyComponent(
			const ComponentDesc& desc
		);

		void setVelocity(
			const Vec3& velocity
		) noexcept;

		Vec3 getVelocity() const noexcept;

		void setAngularVelocity(
			const Vec3& angularVelocity
		) noexcept;

		Vec3 getAngularVelocity() const noexcept;

		void setMass(f32 mass) noexcept;
		f32 getMass() const noexcept;

		void setRestitution(
			f32 restitution
		) noexcept;

		f32 getRestitution() const noexcept;

		void setFriction(
			f32 friction
		) noexcept;

		f32 getFriction() const noexcept;

		void setUseGravity(
			bool useGravity
		) noexcept;

		bool getUseGravity() const noexcept;

		void setStatic(
			bool isStatic
		) noexcept;

		bool getStatic() const noexcept;

	private:
		Vec3 m_velocity{};
		Vec3 m_angularVelocity{};

		f32 m_mass{ 1.0f };
		f32 m_restitution{ 0.45f };
		f32 m_friction{ 0.20f };

		bool m_useGravity{ true };
		bool m_isStatic{ false };

		Vec3 m_initialPosition{};
		Vec3 m_initialRotation{};
		Vec3 m_initialScale{ 1.0f, 1.0f, 1.0f };

		Vec3 m_initialVelocity{};
		Vec3 m_initialAngularVelocity{};

		bool m_hasInitialState{ false };
	};
}