#pragma once

#include <DX3D/Game/Component.h>

namespace dx3d
{
	enum class RigidBodyType : ui32 { Static = 0, Dynamic, Kinematic };

	class RigidBodyComponent final : public Component
	{
		dx3d_typeid(RigidBodyComponent)
	public:
		explicit RigidBodyComponent(const ComponentDesc& data);

		void setBodyType(RigidBodyType value) noexcept;
		RigidBodyType getBodyType() const noexcept;
		void setFriction(f32 value) noexcept;
		f32 getFriction() const noexcept;
		void setRestitution(f32 value) noexcept;
		f32 getRestitution() const noexcept;
		void setLinearDamping(f32 value) noexcept;
		f32 getLinearDamping() const noexcept;
		void setAngularDamping(f32 value) noexcept;
		f32 getAngularDamping() const noexcept;
		void setGravityFactor(f32 value) noexcept;
		f32 getGravityFactor() const noexcept;
		void setEnabled(bool value) noexcept;
		bool isEnabled() const noexcept;

	private:
		RigidBodyType m_bodyType{ RigidBodyType::Static };
		f32 m_friction{ 0.5f };
		f32 m_restitution{};
		f32 m_linearDamping{ 0.05f };
		f32 m_angularDamping{ 0.05f };
		f32 m_gravityFactor{ 1.0f };
		bool m_enabled{ true };
	};
}
