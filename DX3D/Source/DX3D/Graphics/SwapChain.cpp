#include <DX3D/Graphics/SwapChain.h>
#include <DX3D/Graphics/GraphicsDevice.h>

#include <algorithm>

dx3d::SwapChain::SwapChain(
	const SwapChainDesc& desc,
	const GraphicsResourceDesc& gDesc
)
	:
	GraphicsResource(gDesc),
	m_size(desc.winSize)
{
	if (!desc.winHandle)
	{
		DX3DLogThrowInvalidArg(
			"No window handle provided."
		);
	}

	DXGI_SWAP_CHAIN_DESC dxgiDesc{};

	dxgiDesc.BufferDesc.Width =
		std::max(
			1,
			desc.winSize.width
		);

	dxgiDesc.BufferDesc.Height =
		std::max(
			1,
			desc.winSize.height
		);

	dxgiDesc.BufferDesc.Format =
		DXGI_FORMAT_R8G8B8A8_UNORM;

	dxgiDesc.BufferCount = 2;

	dxgiDesc.BufferUsage =
		DXGI_USAGE_RENDER_TARGET_OUTPUT;

	dxgiDesc.OutputWindow =
		static_cast<HWND>(
			desc.winHandle
			);

	dxgiDesc.SampleDesc.Count = 1;
	dxgiDesc.SampleDesc.Quality = 0;

	dxgiDesc.SwapEffect =
		DXGI_SWAP_EFFECT_FLIP_DISCARD;

	dxgiDesc.Windowed = TRUE;

	DX3DGraphicsLogThrowOnFail(
		m_factory.CreateSwapChain(
			&m_device,
			&dxgiDesc,
			&m_swapChain
		),
		"CreateSwapChain failed."
	);

	reloadBuffers();
}

dx3d::Rect
dx3d::SwapChain::getSize() const noexcept
{
	return m_size;
}

void dx3d::SwapChain::resize(
	const Rect& size
)
{
	if (
		size.width <= 0 ||
		size.height <= 0
		)
	{
		return;
	}

	if (
		size.width == m_size.width &&
		size.height == m_size.height
		)
	{
		return;
	}

	auto* immediateContext =
		m_graphicsDevice->
		getImmediateContext();

	immediateContext->OMSetRenderTargets(
		0,
		nullptr,
		nullptr
	);

	immediateContext->Flush();

	m_rtv.Reset();
	m_dsv.Reset();

	DX3DGraphicsLogThrowOnFail(
		m_swapChain->ResizeBuffers(
			0,
			static_cast<UINT>(
				size.width
				),
			static_cast<UINT>(
				size.height
				),
			DXGI_FORMAT_UNKNOWN,
			0
		),
		"ResizeBuffers failed."
	);

	m_size =
		size;

	reloadBuffers();
}

void dx3d::SwapChain::present(
	bool vsync
)
{
	const auto result =
		m_swapChain->Present(
			vsync ? 1 : 0,
			0
		);

	if (FAILED(result))
	{
		DX3DLogError(
			"Present failed."
		);
	}
}

void dx3d::SwapChain::resizeSceneFrame(const Rect& size)
{
	if (size.width <= 0 || size.height <= 0 || size == m_sceneFrameSize)
		return;

	m_sceneFrameSize = size;
	reloadSceneFrame();
}

dx3d::Rect dx3d::SwapChain::getSceneFrameSize() const noexcept
{
	return m_sceneFrameSize;
}

ID3D11ShaderResourceView* dx3d::SwapChain::getSceneFrameView() const noexcept
{
	return m_sceneFrameView.Get();
}

void dx3d::SwapChain::resizeGameFrame(const Rect& size)
{
	if (size.width <= 0 || size.height <= 0 || size == m_gameFrameSize)
		return;

	m_gameFrameSize = size;
	reloadGameFrame();
}

dx3d::Rect dx3d::SwapChain::getGameFrameSize() const noexcept
{
	return m_gameFrameSize;
}

ID3D11ShaderResourceView* dx3d::SwapChain::getGameFrameView() const noexcept
{
	return m_gameFrameView.Get();
}

void dx3d::SwapChain::reloadBuffers()
{
	Microsoft::WRL::ComPtr<
		ID3D11Texture2D
	> backBuffer{};

	DX3DGraphicsLogThrowOnFail(
		m_swapChain->GetBuffer(
			0,
			IID_PPV_ARGS(
				&backBuffer
			)
		),
		"GetBuffer failed."
	);

	DX3DGraphicsLogThrowOnFail(
		m_device.CreateRenderTargetView(
			backBuffer.Get(),
			nullptr,
			&m_rtv
		),
		"CreateRenderTargetView failed."
	);

	D3D11_TEXTURE2D_DESC
		depthTextureDesc{};

	depthTextureDesc.Width =
		static_cast<UINT>(
			std::max(
				1,
				m_size.width
			)
			);

	depthTextureDesc.Height =
		static_cast<UINT>(
			std::max(
				1,
				m_size.height
			)
			);

	depthTextureDesc.Format =
		DXGI_FORMAT_D24_UNORM_S8_UINT;

	depthTextureDesc.BindFlags =
		D3D11_BIND_DEPTH_STENCIL;

	depthTextureDesc.MipLevels = 1;
	depthTextureDesc.ArraySize = 1;

	depthTextureDesc.SampleDesc.Count = 1;
	depthTextureDesc.SampleDesc.Quality = 0;

	depthTextureDesc.Usage =
		D3D11_USAGE_DEFAULT;

	Microsoft::WRL::ComPtr<
		ID3D11Texture2D
	> depthBuffer{};

	DX3DGraphicsLogThrowOnFail(
		m_device.CreateTexture2D(
			&depthTextureDesc,
			nullptr,
			&depthBuffer
		),
		"CreateTexture2D failed."
	);

	DX3DGraphicsLogThrowOnFail(
		m_device.CreateDepthStencilView(
			depthBuffer.Get(),
			nullptr,
			&m_dsv
		),
		"CreateDepthStencilView failed."
	);

	if (!m_sceneFrame) reloadSceneFrame();
	if (!m_gameFrame) reloadGameFrame();
}

void dx3d::SwapChain::reloadSceneFrame()
{
	m_sceneFrameView.Reset();
	m_sceneFrameTarget.Reset();
	m_sceneFrameDepth.Reset();
	m_sceneFrame.Reset();

	D3D11_TEXTURE2D_DESC colorDesc{};
	colorDesc.Width = static_cast<UINT>((std::max)(1, m_sceneFrameSize.width));
	colorDesc.Height = static_cast<UINT>((std::max)(1, m_sceneFrameSize.height));
	colorDesc.MipLevels = 1;
	colorDesc.ArraySize = 1;
	colorDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	colorDesc.SampleDesc.Count = 1;
	colorDesc.Usage = D3D11_USAGE_DEFAULT;
	colorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	DX3DGraphicsLogThrowOnFail(
		m_device.CreateTexture2D(&colorDesc, nullptr, &m_sceneFrame),
		"Create scene viewport texture failed.");
	DX3DGraphicsLogThrowOnFail(
		m_device.CreateRenderTargetView(m_sceneFrame.Get(), nullptr, &m_sceneFrameTarget),
		"Create scene viewport render target failed.");
	DX3DGraphicsLogThrowOnFail(
		m_device.CreateShaderResourceView(m_sceneFrame.Get(), nullptr, &m_sceneFrameView),
		"Create scene viewport view failed.");

	D3D11_TEXTURE2D_DESC depthDesc = colorDesc;
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture;
	DX3DGraphicsLogThrowOnFail(
		m_device.CreateTexture2D(&depthDesc, nullptr, &depthTexture),
		"Create scene viewport depth texture failed.");
	DX3DGraphicsLogThrowOnFail(
		m_device.CreateDepthStencilView(depthTexture.Get(), nullptr, &m_sceneFrameDepth),
		"Create scene viewport depth view failed.");
}

void dx3d::SwapChain::reloadGameFrame()
{
	m_gameFrameView.Reset();
	m_gameFrameTarget.Reset();
	m_gameFrameDepth.Reset();
	m_gameFrame.Reset();

	D3D11_TEXTURE2D_DESC colorDesc{};
	colorDesc.Width = static_cast<UINT>((std::max)(1, m_gameFrameSize.width));
	colorDesc.Height = static_cast<UINT>((std::max)(1, m_gameFrameSize.height));
	colorDesc.MipLevels = 1;
	colorDesc.ArraySize = 1;
	colorDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	colorDesc.SampleDesc.Count = 1;
	colorDesc.Usage = D3D11_USAGE_DEFAULT;
	colorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	DX3DGraphicsLogThrowOnFail(
		m_device.CreateTexture2D(&colorDesc, nullptr, &m_gameFrame),
		"Create game viewport texture failed.");
	DX3DGraphicsLogThrowOnFail(
		m_device.CreateRenderTargetView(m_gameFrame.Get(), nullptr, &m_gameFrameTarget),
		"Create game viewport render target failed.");
	DX3DGraphicsLogThrowOnFail(
		m_device.CreateShaderResourceView(m_gameFrame.Get(), nullptr, &m_gameFrameView),
		"Create game viewport view failed.");

	D3D11_TEXTURE2D_DESC depthDesc = colorDesc;
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture;
	DX3DGraphicsLogThrowOnFail(
		m_device.CreateTexture2D(&depthDesc, nullptr, &depthTexture),
		"Create game viewport depth texture failed.");
	DX3DGraphicsLogThrowOnFail(
		m_device.CreateDepthStencilView(depthTexture.Get(), nullptr, &m_gameFrameDepth),
		"Create game viewport depth view failed.");
}
