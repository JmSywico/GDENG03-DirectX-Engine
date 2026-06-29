#pragma once

#include <DX3D/Graphics/GraphicsResource.h>

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

	private:
		Microsoft::WRL::ComPtr<
			ID3D11Texture2D
		> m_texture{};

		Microsoft::WRL::ComPtr<
			ID3D11DepthStencilView
		> m_depthStencilView{};

		Microsoft::WRL::ComPtr<
			ID3D11ShaderResourceView
		> m_shaderResourceView{};

		Microsoft::WRL::ComPtr<
			ID3D11SamplerState
		> m_samplerState{};

		ui32 m_width{};
		ui32 m_height{};

		friend class DeviceContext;
	};
}
