#pragma once

#include <DX3D/Game/Component.h>

namespace dx3d
{
	class FlyControllerComponent final : public Component
	{
		dx3d_typeid(FlyControllerComponent)
	public:
		explicit FlyControllerComponent(const ComponentDesc& data) : Component(data) {}
		void setMoveSpeed(f32 value) noexcept { if (value >= 0.0f) m_moveSpeed = value; }
		f32 getMoveSpeed() const noexcept { return m_moveSpeed; }
		void setLookSensitivity(f32 value) noexcept { if (value >= 0.0f) m_lookSensitivity = value; }
		f32 getLookSensitivity() const noexcept { return m_lookSensitivity; }
		void setBoostMultiplier(f32 value) noexcept { if (value >= 1.0f) m_boostMultiplier = value; }
		f32 getBoostMultiplier() const noexcept { return m_boostMultiplier; }
		void setPitchLimitDegrees(f32 value) noexcept { if (value > 0.0f && value <= 90.0f) m_pitchLimitDegrees = value; }
		f32 getPitchLimitDegrees() const noexcept { return m_pitchLimitDegrees; }
		void setEnabled(bool value) noexcept { m_enabled = value; }
		bool isEnabled() const noexcept { return m_enabled; }
	private:
		f32 m_moveSpeed{ 5.0f };
		f32 m_lookSensitivity{ 0.0025f };
		f32 m_boostMultiplier{ 4.0f };
		f32 m_pitchLimitDegrees{ 89.0f };
		bool m_enabled{ true };
	};
}
