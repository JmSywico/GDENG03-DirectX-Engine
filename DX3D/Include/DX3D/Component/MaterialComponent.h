#pragma once

#include <DX3D/Core/Core.h>
#include <DX3D/Game/Component.h>
#include <DX3D/Math/Vec2.h>
#include <DX3D/Math/Vec4.h>

#include <string>

namespace dx3d
{
	class MaterialComponent final : public Component
	{
		dx3d_typeid(MaterialComponent)

	public:
		explicit MaterialComponent(
			const ComponentDesc& data
		);

		void setTexturePath(
			const std::string& texturePath
		);

		const std::string&
			getTexturePath() const noexcept;

		bool hasTexture() const noexcept;

		void clearTexture();

		void setUvTiling(
			const Vec2& uvTiling
		) noexcept;

		const Vec2& getUvTiling() const noexcept;

		void setUvOffset(
			const Vec2& uvOffset
		) noexcept;

		const Vec2& getUvOffset() const noexcept;

		void setColor(
			const Vec4& color
		) noexcept;

		const Vec4& getColor() const noexcept;

	private:
		std::string m_texturePath{};
		Vec2 m_uvTiling{ 1.0f, 1.0f };
		Vec2 m_uvOffset{};
		Vec4 m_color{
			1.0f,
			1.0f,
			1.0f,
			1.0f
		};
	};
}
