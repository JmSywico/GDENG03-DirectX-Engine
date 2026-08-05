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
	m_sceneFrameView.Reset();
	m_sceneFrame.Reset();
	m_gameFrameView.Reset();
	m_gameFrame.Reset();

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

void dx3d::SwapChain::captureSceneFrame()
{
	if (!m_sceneFrame) return;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
	if (FAILED(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) return;
	m_graphicsDevice->getImmediateContext()->CopyResource(
		m_sceneFrame.Get(), backBuffer.Get());
}

ID3D11ShaderResourceView* dx3d::SwapChain::getSceneFrameView() const noexcept
{
	return m_sceneFrameView.Get();
}

void dx3d::SwapChain::captureGameFrame()
{
	if (!m_gameFrame) return;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
	if (FAILED(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) return;
	m_graphicsDevice->getImmediateContext()->CopyResource(
		m_gameFrame.Get(), backBuffer.Get());
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

	D3D11_TEXTURE2D_DESC sceneFrameDesc{};
	backBuffer->GetDesc(&sceneFrameDesc);
	sceneFrameDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	sceneFrameDesc.MiscFlags = 0;
	DX3DGraphicsLogThrowOnFail(
		m_device.CreateTexture2D(&sceneFrameDesc, nullptr, &m_sceneFrame),
		"Create scene viewport texture failed.");
	DX3DGraphicsLogThrowOnFail(
		m_device.CreateShaderResourceView(m_sceneFrame.Get(), nullptr, &m_sceneFrameView),
		"Create scene viewport view failed.");
	DX3DGraphicsLogThrowOnFail(
		m_device.CreateTexture2D(&sceneFrameDesc, nullptr, &m_gameFrame),
		"Create game viewport texture failed.");
	DX3DGraphicsLogThrowOnFail(
		m_device.CreateShaderResourceView(m_gameFrame.Get(), nullptr, &m_gameFrameView),
		"Create game viewport view failed.");

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
}
