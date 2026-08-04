#pragma once

#include <DX3D/Game/Component.h>
#include <DX3D/Math/Vec3.h>
#include <DX3D/Math/Vec4.h>

namespace dx3d
{
	enum class MaterialMode : ui32
	{
		LitTint = 0,
		RainbowDebug,
		FlatRed,
		FlatGreen,
		FlatBlue
	};

	class MaterialComponent final : public Component
	{
		dx3d_typeid(MaterialComponent)

	public:
		explicit MaterialComponent(const ComponentDesc& data);

		void setMode(MaterialMode mode) noexcept;
		MaterialMode getMode() const noexcept;
		void setAlbedo(const Vec4& albedo) noexcept;
		Vec4 getAlbedo() const noexcept;
		void setEmissive(const Vec3& emissive) noexcept;
		Vec3 getEmissive() const noexcept;
		void setEmissionStrength(f32 strength) noexcept;
		f32 getEmissionStrength() const noexcept;

	private:
		MaterialMode m_mode{ MaterialMode::LitTint };
		Vec4 m_albedo{ 1.0f, 1.0f, 1.0f, 1.0f };
		Vec3 m_emissive{};
		f32 m_emissionStrength{};
	};
}
