#pragma once

#include <DX3D/Graphics/GraphicsResource.h>

namespace dx3d
{
	class SwapChain final :
		public GraphicsResource
	{
	public:
		SwapChain(
			const SwapChainDesc& desc,
			const GraphicsResourceDesc& gDesc
		);

		Rect getSize() const noexcept;

		void resize(
			const Rect& size
		);

		void present(
			bool vsync = false
		);
		void captureSceneFrame();
		ID3D11ShaderResourceView* getSceneFrameView() const noexcept;
		void captureGameFrame();
		ID3D11ShaderResourceView* getGameFrameView() const noexcept;

	private:
		void reloadBuffers();

	private:
		Microsoft::WRL::ComPtr<
			IDXGISwapChain
		> m_swapChain{};

		Microsoft::WRL::ComPtr<
			ID3D11RenderTargetView
		> m_rtv{};

		Microsoft::WRL::ComPtr<
			ID3D11DepthStencilView
		> m_dsv{};
		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_sceneFrame{};
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_sceneFrameView{};
		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_gameFrame{};
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_gameFrameView{};

		Rect m_size{};

		friend class DeviceContext;
		friend class GraphicsDevice;
	};
}
