#include <DX3D/Graphics/GraphicsDevice.h>
#include <DX3D/Graphics/GraphicsLogUtils.h>
#include <DX3D/Graphics/SwapChain.h>
#include <DX3D/Graphics/DeviceContext.h>
#include <DX3D/Graphics/ShaderBinary.h>
#include <DX3D/Graphics/GraphicsPipelineState.h>
#include <DX3D/Graphics/VertexBuffer.h>
#include <DX3D/Graphics/IndexBuffer.h>
#include <DX3D/Graphics/VertexShaderSignature.h>
#include <DX3D/Graphics/ConstantBuffer.h>
#include <DX3D/Graphics/ShadowMap.h>
#include <DX3D/Graphics/Texture2D.h>

using namespace dx3d;

dx3d::GraphicsDevice::GraphicsDevice(
	const GraphicsDeviceDesc& desc
)
	: Base(desc.base)
{
	D3D_FEATURE_LEVEL featureLevel{};
	UINT createDeviceFlags{};

#ifdef _DEBUG
	createDeviceFlags |=
		D3D11_CREATE_DEVICE_DEBUG;
#endif

	DX3DGraphicsLogThrowOnFail(
		D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			createDeviceFlags,
			nullptr,
			0,
			D3D11_SDK_VERSION,
			&m_d3dDevice,
			&featureLevel,
			&m_d3dContext
		),
		"Direct3D11 initialization failed."
	);

	DX3DGraphicsLogThrowOnFail(
		m_d3dDevice->QueryInterface(
			IID_PPV_ARGS(
				&m_dxgiDevice
			)
		),
		"QueryInterface failed to retrieve IDXGIDevice."
	);

	DX3DGraphicsLogThrowOnFail(
		m_dxgiDevice->GetParent(
			IID_PPV_ARGS(
				&m_dxgiAdapter
			)
		),
		"GetParent failed to retrieve IDXGIAdapter."
	);

	DX3DGraphicsLogThrowOnFail(
		m_dxgiAdapter->GetParent(
			IID_PPV_ARGS(
				&m_dxgiFactory
			)
		),
		"GetParent failed to retrieve IDXGIFactory."
	);
}

dx3d::GraphicsDevice::~GraphicsDevice()
{}

ID3D11Device*
dx3d::GraphicsDevice::getD3DDevice() const noexcept
{
	return m_d3dDevice.Get();
}

ID3D11DeviceContext*
dx3d::GraphicsDevice::getImmediateContext() const noexcept
{
	return m_d3dContext.Get();
}

RefPtr<SwapChain>
dx3d::GraphicsDevice::createSwapChain(
	const SwapChainDesc& desc
)
{
	return std::make_shared<SwapChain>(
		desc,
		getGraphicsResourceDesc()
	);
}

RefPtr<DeviceContext>
dx3d::GraphicsDevice::createDeviceContext()
{
	return std::make_shared<DeviceContext>(
		getGraphicsResourceDesc()
	);
}

RefPtr<ShaderBinary>
dx3d::GraphicsDevice::compileShader(
	const ShaderCompileDesc& desc
)
{
	return std::make_shared<ShaderBinary>(
		desc,
		getGraphicsResourceDesc()
	);
}

RefPtr<GraphicsPipelineState>
dx3d::GraphicsDevice::createGraphicsPipelineState(
	const GraphicsPipelineStateDesc& desc
)
{
	return std::make_shared<
		GraphicsPipelineState
	>(
		desc,
		getGraphicsResourceDesc()
	);
}

RefPtr<VertexBuffer>
dx3d::GraphicsDevice::createVertexBuffer(
	const VertexBufferDesc& desc
)
{
	return std::make_shared<VertexBuffer>(
		desc,
		getGraphicsResourceDesc()
	);
}

RefPtr<VertexShaderSignature>
dx3d::GraphicsDevice::createVertexShaderSignature(
	const VertexShaderSignatureDesc& desc
)
{
	return std::make_shared<
		VertexShaderSignature
	>(
		desc,
		getGraphicsResourceDesc()
	);
}

RefPtr<ConstantBuffer>
dx3d::GraphicsDevice::createConstantBuffer(
	const ConstantBufferDesc& desc
)
{
	return std::make_shared<ConstantBuffer>(
		desc,
		getGraphicsResourceDesc()
	);
}

RefPtr<IndexBuffer>
dx3d::GraphicsDevice::createIndexBuffer(
	const IndexBufferDesc& desc
)
{
	return std::make_shared<IndexBuffer>(
		desc,
		getGraphicsResourceDesc()
	);
}

RefPtr<ShadowMap>
dx3d::GraphicsDevice::createShadowMap(
	const ShadowMapDesc& desc
)
{
	return std::make_shared<ShadowMap>(
		desc,
		getGraphicsResourceDesc()
	);
}

RefPtr<Texture2D>
dx3d::GraphicsDevice::createTexture2D(
	const Texture2DDesc& desc
)
{
	return std::make_shared<Texture2D>(
		desc,
		getGraphicsResourceDesc()
	);
}

void dx3d::GraphicsDevice::executeCommandList(
	DeviceContext& context
)
{
	Microsoft::WRL::ComPtr<
		ID3D11CommandList
	> list{};

	auto result =
		context.m_context->
		FinishCommandList(
			false,
			&list
		);

	if (FAILED(result))
	{
		DX3DLogError(
			"FinishCommandList failed."
		);

		return;
	}

	m_d3dContext->ExecuteCommandList(
		list.Get(),
		false
	);
}

void dx3d::GraphicsDevice::bindBackBuffer(
	const SwapChain& swapChain
)
{
	auto renderTargetView =
		swapChain.m_rtv.Get();

	auto depthStencilView =
		swapChain.m_dsv.Get();

	m_d3dContext->OMSetRenderTargets(
		1,
		&renderTargetView,
		depthStencilView
	);
}

GraphicsResourceDesc
dx3d::GraphicsDevice::
getGraphicsResourceDesc() const noexcept
{
	return
	{
		{ m_logger },
		shared_from_this(),
		*m_d3dDevice.Get(),
		*m_dxgiFactory.Get()
	};
}
