#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <cstdint>
#include <memory>

namespace enignE::Graphics::DX11
{
	struct ContextStats
	{
		double PresentCpuMs = 0.0;
		std::uint64_t VideoMemoryUsageBytes = 0;
		std::uint64_t VideoMemoryBudgetBytes = 0;
		std::uint64_t PresentCount = 0;
	};

	class RenderTarget final
	{
	public:
		bool Initialize(ID3D11Device& device, std::uint16_t width, std::uint16_t height,
			DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM);
		void Reset();
		bool IsValid() const { return m_color && m_rtv && m_srv && m_depth && m_dsv; }
		std::uint16_t GetWidth() const { return m_width; }
		std::uint16_t GetHeight() const { return m_height; }
		ID3D11RenderTargetView* GetRTV() const { return m_rtv.Get(); }
		ID3D11DepthStencilView* GetDSV() const { return m_dsv.Get(); }
		ID3D11ShaderResourceView* GetSRV() const { return m_srv.Get(); }

	private:
		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_color;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_srv;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_depth;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_dsv;
		std::uint16_t m_width = 0;
		std::uint16_t m_height = 0;
	};

	class Context final
	{
	public:
		Context() = default;
		~Context();
		Context(const Context&) = delete;
		Context& operator=(const Context&) = delete;

		bool Initialize(HWND window, std::uint16_t width, std::uint16_t height);
		void Shutdown();
		bool Resize(std::uint16_t width, std::uint16_t height);
		void BeginBackBuffer(const float clearColor[4]);
		void Bind(RenderTarget& target, const float clearColor[4]);
		void Present(bool verticalSync);
		void RefreshMemoryStats();

		ID3D11Device& GetDevice() const { return *m_device.Get(); }
		ID3D11DeviceContext& GetImmediateContext() const { return *m_context.Get(); }
		IDXGISwapChain1& GetSwapChain() const { return *m_swapChain.Get(); }
		std::uint16_t GetWidth() const { return m_width; }
		std::uint16_t GetHeight() const { return m_height; }
		bool IsInitialized() const { return m_device && m_context && m_swapChain && m_backBufferRTV; }
		const ContextStats& GetStats() const { return m_stats; }

		static Context* GetActive() { return s_active; }

	private:
		bool CreateBackBuffer();
		void BindViews(ID3D11RenderTargetView* rtv, ID3D11DepthStencilView* dsv,
			std::uint16_t width, std::uint16_t height);

		Microsoft::WRL::ComPtr<ID3D11Device> m_device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
		Microsoft::WRL::ComPtr<IDXGISwapChain1> m_swapChain;
		Microsoft::WRL::ComPtr<IDXGIAdapter3> m_adapter;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_backBufferRTV;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_backBufferDepth;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_backBufferDSV;
		std::uint16_t m_width = 0;
		std::uint16_t m_height = 0;
		ContextStats m_stats{};
		static Context* s_active;
	};

	class DepthTarget final
	{
	public:
		bool Initialize(ID3D11Device& device, std::uint16_t width, std::uint16_t height);
		void Reset();
		bool IsValid() const { return m_texture && m_dsv && m_srv; }
		ID3D11DepthStencilView* GetDSV() const { return m_dsv.Get(); }
		ID3D11ShaderResourceView* GetSRV() const { return m_srv.Get(); }
		std::uint16_t GetWidth() const { return m_width; }
		std::uint16_t GetHeight() const { return m_height; }
	private:
		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_texture;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_dsv;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_srv;
		std::uint16_t m_width = 0;
		std::uint16_t m_height = 0;
	};
}
