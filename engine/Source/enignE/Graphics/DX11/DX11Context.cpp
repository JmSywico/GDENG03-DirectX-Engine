#include "Graphics/DX11/DX11Context.h"

#include <algorithm>
#include <chrono>
#include <iterator>

namespace enignE::Graphics::DX11
{
	Context* Context::s_active = nullptr;

	bool RenderTarget::Initialize(ID3D11Device& device, std::uint16_t width,
		std::uint16_t height, DXGI_FORMAT colorFormat)
	{
		Reset();
		if (width == 0 || height == 0) return false;

		D3D11_TEXTURE2D_DESC color{};
		color.Width = width;
		color.Height = height;
		color.MipLevels = 1;
		color.ArraySize = 1;
		color.Format = colorFormat;
		color.SampleDesc.Count = 1;
		color.Usage = D3D11_USAGE_DEFAULT;
		color.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		if (FAILED(device.CreateTexture2D(&color, nullptr, &m_color)) ||
			FAILED(device.CreateRenderTargetView(m_color.Get(), nullptr, &m_rtv)) ||
			FAILED(device.CreateShaderResourceView(m_color.Get(), nullptr, &m_srv)))
		{
			Reset();
			return false;
		}

		D3D11_TEXTURE2D_DESC depth = color;
		depth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		if (FAILED(device.CreateTexture2D(&depth, nullptr, &m_depth)) ||
			FAILED(device.CreateDepthStencilView(m_depth.Get(), nullptr, &m_dsv)))
		{
			Reset();
			return false;
		}
		m_width = width;
		m_height = height;
		return true;
	}

	void RenderTarget::Reset()
	{
		m_dsv.Reset();
		m_depth.Reset();
		m_srv.Reset();
		m_rtv.Reset();
		m_color.Reset();
		m_width = 0;
		m_height = 0;
	}

	Context::~Context() { Shutdown(); }

	bool Context::Initialize(HWND window, std::uint16_t width, std::uint16_t height)
	{
		Shutdown();
		if (!window || width == 0 || height == 0 || (s_active && s_active != this)) return false;

		UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
		flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
		D3D_FEATURE_LEVEL requested[]{ D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
		D3D_FEATURE_LEVEL selected{};
		if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
			requested, static_cast<UINT>(std::size(requested)), D3D11_SDK_VERSION,
			&m_device, &selected, &m_context))) return false;

		Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
		Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
		Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
		if (FAILED(m_device.As(&dxgiDevice)) || FAILED(dxgiDevice->GetAdapter(&adapter)) ||
			FAILED(adapter->GetParent(IID_PPV_ARGS(&factory))))
		{
			Shutdown();
			return false;
		}
		adapter.As(&m_adapter);

		DXGI_SWAP_CHAIN_DESC1 swap{};
		swap.Width = width;
		swap.Height = height;
		swap.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swap.SampleDesc.Count = 1;
		swap.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swap.BufferCount = 2;
		swap.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		if (FAILED(factory->CreateSwapChainForHwnd(m_device.Get(), window, &swap,
			nullptr, nullptr, &m_swapChain)))
		{
			Shutdown();
			return false;
		}
		factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
		m_width = width;
		m_height = height;
		if (!CreateBackBuffer())
		{
			Shutdown();
			return false;
		}
		s_active = this;
		return true;
	}

	void Context::Shutdown()
	{
		if (m_context) m_context->ClearState();
		m_backBufferDSV.Reset();
		m_backBufferDepth.Reset();
		m_backBufferRTV.Reset();
		m_swapChain.Reset();
		m_adapter.Reset();
		m_context.Reset();
		m_device.Reset();
		m_width = m_height = 0;
		m_stats = {};
		if (s_active == this) s_active = nullptr;
	}

	bool Context::Resize(std::uint16_t width, std::uint16_t height)
	{
		if (!IsInitialized() || width == 0 || height == 0) return false;
		if (width == m_width && height == m_height) return true;
		m_context->OMSetRenderTargets(0, nullptr, nullptr);
		m_backBufferDSV.Reset();
		m_backBufferDepth.Reset();
		m_backBufferRTV.Reset();
		if (FAILED(m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0))) return false;
		m_width = width;
		m_height = height;
		return CreateBackBuffer();
	}

	bool Context::CreateBackBuffer()
	{
		Microsoft::WRL::ComPtr<ID3D11Texture2D> color;
		if (FAILED(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&color))) ||
			FAILED(m_device->CreateRenderTargetView(color.Get(), nullptr, &m_backBufferRTV))) return false;
		D3D11_TEXTURE2D_DESC depth{};
		depth.Width = m_width;
		depth.Height = m_height;
		depth.MipLevels = 1;
		depth.ArraySize = 1;
		depth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depth.SampleDesc.Count = 1;
		depth.Usage = D3D11_USAGE_DEFAULT;
		depth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		return SUCCEEDED(m_device->CreateTexture2D(&depth, nullptr, &m_backBufferDepth)) &&
			SUCCEEDED(m_device->CreateDepthStencilView(m_backBufferDepth.Get(), nullptr, &m_backBufferDSV));
	}

	void Context::BindViews(ID3D11RenderTargetView* rtv, ID3D11DepthStencilView* dsv,
		std::uint16_t width, std::uint16_t height)
	{
		m_context->OMSetRenderTargets(1, &rtv, dsv);
		D3D11_VIEWPORT viewport{};
		viewport.Width = static_cast<float>(width);
		viewport.Height = static_cast<float>(height);
		viewport.MaxDepth = 1.0f;
		m_context->RSSetViewports(1, &viewport);
	}

	void Context::BeginBackBuffer(const float clearColor[4])
	{
		BindViews(m_backBufferRTV.Get(), m_backBufferDSV.Get(), m_width, m_height);
		m_context->ClearRenderTargetView(m_backBufferRTV.Get(), clearColor);
		m_context->ClearDepthStencilView(m_backBufferDSV.Get(),
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}

	void Context::Bind(RenderTarget& target, const float clearColor[4])
	{
		if (!target.IsValid()) return;
		BindViews(target.GetRTV(), target.GetDSV(), target.GetWidth(), target.GetHeight());
		m_context->ClearRenderTargetView(target.GetRTV(), clearColor);
		m_context->ClearDepthStencilView(target.GetDSV(),
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}

	void Context::Present(bool verticalSync)
	{
		if (!m_swapChain) return;
		const auto started = std::chrono::steady_clock::now();
		m_swapChain->Present(verticalSync ? 1u : 0u, 0);
		m_stats.PresentCpuMs = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - started).count();
		++m_stats.PresentCount;
		RefreshMemoryStats();
	}

	void Context::RefreshMemoryStats()
	{
		if (!m_adapter) return;
		DXGI_QUERY_VIDEO_MEMORY_INFO info{};
		if (SUCCEEDED(m_adapter->QueryVideoMemoryInfo(
			0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info)))
		{
			m_stats.VideoMemoryUsageBytes = info.CurrentUsage;
			m_stats.VideoMemoryBudgetBytes = info.Budget;
		}
	}

	bool DepthTarget::Initialize(ID3D11Device& device, std::uint16_t width, std::uint16_t height)
	{
		Reset();
		if (width == 0 || height == 0) return false;
		D3D11_TEXTURE2D_DESC texture{};
		texture.Width = width;
		texture.Height = height;
		texture.MipLevels = 1;
		texture.ArraySize = 1;
		texture.Format = DXGI_FORMAT_R32_TYPELESS;
		texture.SampleDesc.Count = 1;
		texture.Usage = D3D11_USAGE_DEFAULT;
		texture.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		D3D11_DEPTH_STENCIL_VIEW_DESC dsv{};
		dsv.Format = DXGI_FORMAT_D32_FLOAT;
		dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		D3D11_SHADER_RESOURCE_VIEW_DESC srv{};
		srv.Format = DXGI_FORMAT_R32_FLOAT;
		srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv.Texture2D.MipLevels = 1;
		if (FAILED(device.CreateTexture2D(&texture, nullptr, &m_texture)) ||
			FAILED(device.CreateDepthStencilView(m_texture.Get(), &dsv, &m_dsv)) ||
			FAILED(device.CreateShaderResourceView(m_texture.Get(), &srv, &m_srv)))
		{
			Reset();
			return false;
		}
		m_width = width;
		m_height = height;
		return true;
	}

	void DepthTarget::Reset()
	{
		m_srv.Reset();
		m_dsv.Reset();
		m_texture.Reset();
		m_width = m_height = 0;
	}
}
