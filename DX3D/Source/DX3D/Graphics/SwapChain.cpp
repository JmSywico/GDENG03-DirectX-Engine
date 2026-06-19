#include <DX3D/Graphics/SwapChain.h>

dx3d::SwapChain::SwapChain(const SwapChainDesc& desc, const GraphicsResourceDesc& gDesc) : 
	GraphicsResource(gDesc), m_size(desc.winSize)
{
	if (!desc.winHandle) DX3DLogThrowInvalidArg("No window handle provided.");

	DXGI_SWAP_CHAIN_DESC dxgiDesc{};

	dxgiDesc.BufferDesc.Width = std::max(1, desc.winSize.width);
	dxgiDesc.BufferDesc.Height = std::max(1, desc.winSize.height);
	dxgiDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dxgiDesc.BufferCount = 2;
	dxgiDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

	dxgiDesc.OutputWindow = static_cast<HWND>(desc.winHandle);
	dxgiDesc.SampleDesc.Count = 1;
	dxgiDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	dxgiDesc.Windowed = TRUE;

	DX3DGraphicsLogThrowOnFail(m_factory.CreateSwapChain(&m_device, &dxgiDesc, &m_swapChain),
		"CreateSwapChain failed.");

	reloadBuffers();
}

dx3d::Rect dx3d::SwapChain::getSize() const noexcept
{
	return m_size;
}

void dx3d::SwapChain::present(bool vsync)
{
	auto hr = m_swapChain->Present(vsync, 0);
	if (FAILED(hr))
	{
		DX3DLogError("Present failed.");
		return;
	}
}

void dx3d::SwapChain::reloadBuffers()
{
	Microsoft::WRL::ComPtr<ID3D11Texture2D> buffer{};
	DX3DGraphicsLogThrowOnFail(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&buffer)),
		"GetBuffer failed.");
	DX3DGraphicsLogThrowOnFail(m_device.CreateRenderTargetView(buffer.Get(), nullptr, &m_rtv),
		"CreateRenderTargetView failed.");

	D3D11_TEXTURE2D_DESC depthTexDesc = {};
	depthTexDesc.Width = std::max(1, m_size.width);
	depthTexDesc.Height = std::max(1, m_size.height);
	depthTexDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthTexDesc.MipLevels = 1;
	depthTexDesc.SampleDesc.Count = 1;
	depthTexDesc.ArraySize = 1;

	DX3DGraphicsLogThrowOnFail(m_device.CreateTexture2D(&depthTexDesc, nullptr, &buffer),
		"CreateTexture2D failed.");
	DX3DGraphicsLogThrowOnFail(m_device.CreateDepthStencilView(buffer.Get(), NULL, &m_dsv),
		"CreateDepthStencilView failed.");
}

