#pragma once

#include <DX3D/Core/Core.h>
#include <DX3D/Game/Component.h>
#include <DX3D/Math/Vec3.h>

namespace dx3d
{
	class DirectionalLightComponent final :
		public Component
	{
		dx3d_typeid(DirectionalLightComponent)

	public:
		explicit DirectionalLightComponent(
			const ComponentDesc& data
		);

		void setColor(
			const Vec3& color
		) noexcept;

		const Vec3& getColor() const noexcept;

		void setIntensity(
			f32 intensity
		) noexcept;

		f32 getIntensity() const noexcept;

		void setAmbientStrength(
			f32 ambientStrength
		) noexcept;

		f32 getAmbientStrength() const noexcept;

		void setShadowArea(
			f32 shadowArea
		) noexcept;

		f32 getShadowArea() const noexcept;

		void setCastShadows(
			bool castShadows
		) noexcept;

		bool getCastShadows() const noexcept;

	private:
		Vec3 m_color
		{
			1.0f,
			1.0f,
			1.0f
		};

		f32 m_intensity{ 1.0f };
		f32 m_ambientStrength{ 0.20f };
		f32 m_shadowArea{ 30.0f };

		bool m_castShadows{ true };
	};
}
