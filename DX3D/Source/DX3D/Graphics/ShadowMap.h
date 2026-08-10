#pragma once

#include <DX3D/Graphics/GraphicsResource.h>

#include <array>

namespace dx3d
{
	class ShadowMap final :
		public GraphicsResource
	{
	public:
		ShadowMap(
			const ShadowMapDesc& desc,
			const GraphicsResourceDesc& graphicsDesc
		);

		ui32 getWidth() const noexcept;
		ui32 getHeight() const noexcept;
		bool isCube() const noexcept;

	private:
		Microsoft::WRL::ComPtr<
			ID3D11Texture2D
		> m_texture{};

		std::array<Microsoft::WRL::ComPtr<
			ID3D11DepthStencilView
		>, 6> m_depthStencilViews{};

		Microsoft::WRL::ComPtr<
			ID3D11ShaderResourceView
		> m_shaderResourceView{};

		Microsoft::WRL::ComPtr<
			ID3D11SamplerState
		> m_samplerState{};

		ui32 m_width{};
		ui32 m_height{};
		bool m_cube{};

		friend class DeviceContext;
	};
}
