#pragma once

#include <DX3D/Game/Component.h>
#include <DX3D/Math/Vec3.h>

namespace dx3d
{
	class RotatorComponent final : public Component
	{
		dx3d_typeid(RotatorComponent)
	public:
		explicit RotatorComponent(const ComponentDesc& data) : Component(data) {}
		void setAngularVelocity(const Vec3& value) noexcept { m_angularVelocity = value; }
		Vec3 getAngularVelocity() const noexcept { return m_angularVelocity; }
		void setEnabled(bool value) noexcept { m_enabled = value; }
		bool isEnabled() const noexcept { return m_enabled; }
	private:
		Vec3 m_angularVelocity{ 0.0f, 1.0f, 0.0f };
		bool m_enabled{ true };
	};
}
