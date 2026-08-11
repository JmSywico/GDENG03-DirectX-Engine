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
		void resizeSceneFrame(const Rect& size);
		Rect getSceneFrameSize() const noexcept;
		ID3D11ShaderResourceView* getSceneFrameView() const noexcept;
		void resizeGameFrame(const Rect& size);
		Rect getGameFrameSize() const noexcept;
		ID3D11ShaderResourceView* getGameFrameView() const noexcept;

	private:
		void reloadBuffers();
		void reloadSceneFrame();
		void reloadGameFrame();

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
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_sceneFrameTarget{};
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_sceneFrameDepth{};
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_sceneFrameView{};
		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_gameFrame{};
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_gameFrameTarget{};
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_gameFrameDepth{};
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_gameFrameView{};

		Rect m_size{};
		Rect m_sceneFrameSize{ 1, 1 };
		Rect m_gameFrameSize{ 1, 1 };

		friend class DeviceContext;
		friend class GraphicsDevice;
	};
}
