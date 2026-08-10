#pragma once

#include <DX3D/Game/Component.h>
#include <string>

namespace dx3d
{
	class TextureComponent final : public Component
	{
		dx3d_typeid(TextureComponent)
	public:
		explicit TextureComponent(const ComponentDesc& data) : Component(data) {}
		void setAssetPath(std::string value) { m_assetPath = std::move(value); }
		const std::string& getAssetPath() const noexcept { return m_assetPath; }
		void setEnabled(bool value) noexcept { m_enabled = value; }
		bool isEnabled() const noexcept { return m_enabled; }
	private:
		std::string m_assetPath{};
		bool m_enabled{ true };
	};
}
