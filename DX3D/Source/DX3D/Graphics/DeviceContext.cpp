#include <DX3D/Graphics/DeviceContext.h>
#include <DX3D/Graphics/SwapChain.h>
#include <DX3D/Graphics/GraphicsPipelineState.h>
#include <DX3D/Graphics/VertexBuffer.h>
#include <DX3D/Graphics/IndexBuffer.h>
#include <DX3D/Graphics/ConstantBuffer.h>
#include <DX3D/Graphics/ShadowMap.h>

dx3d::DeviceContext::DeviceContext(const GraphicsResourceDesc& gDesc): GraphicsResource(gDesc)
{
	DX3DGraphicsLogThrowOnFail(m_device.CreateDeferredContext(0, &m_context),
		"CreateDeferredContext failed.");
}

void dx3d::DeviceContext::clearAndSetBackBuffer(const SwapChain& swapChain, const Vec4& color)
{
	f32 fColor[] = { color.x,color.y,color.z,color.w };
	auto rtv = swapChain.m_rtv.Get();
	auto dsv = swapChain.m_dsv.Get();

	m_context->ClearRenderTargetView(rtv, fColor);
	m_context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
	m_context->OMSetRenderTargets(1, &rtv, dsv);
}

void dx3d::DeviceContext::beginShadowPass(
	const ShadowMap& shadowMap
)
{
	ID3D11ShaderResourceView* nullResource = nullptr;

	m_context->PSSetShaderResources(
		0,
		1,
		&nullResource
	);

	auto depthStencilView =
		shadowMap.m_depthStencilView.Get();

	m_context->ClearDepthStencilView(
		depthStencilView,
		D3D11_CLEAR_DEPTH,
		1.0f,
		0
	);

	m_context->OMSetRenderTargets(
		0,
		nullptr,
		depthStencilView
	);

	D3D11_VIEWPORT viewport{};

	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;

	viewport.Width =
		static_cast<f32>(
			shadowMap.m_width
			);

	viewport.Height =
		static_cast<f32>(
			shadowMap.m_height
			);

	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	m_context->RSSetViewports(
		1,
		&viewport
	);
}

void dx3d::DeviceContext::setShadowMap(
	const ShadowMap& shadowMap
)
{
	auto shaderResourceView =
		shadowMap.m_shaderResourceView.Get();

	auto samplerState =
		shadowMap.m_samplerState.Get();

	m_context->PSSetShaderResources(
		0,
		1,
		&shaderResourceView
	);

	m_context->PSSetSamplers(
		0,
		1,
		&samplerState
	);
}

void dx3d::DeviceContext::setGraphicsPipelineState(const GraphicsPipelineState& pipeline)
{
	m_context->IASetInputLayout(pipeline.m_layout.Get());
	m_context->VSSetShader(pipeline.m_vs.Get(), nullptr, 0);
	m_context->PSSetShader(pipeline.m_ps.Get(), nullptr, 0);
}

void dx3d::DeviceContext::setVertexBuffer(const VertexBuffer& buffer)
{
	auto stride = buffer.m_vertexSize;
	auto buf = buffer.m_buffer.Get();
	auto offset = 0u;
	m_context->IASetVertexBuffers(0, 1, &buf, &stride, &offset);
}

void dx3d::DeviceContext::setIndexBuffer(const IndexBuffer& buffer)
{
	m_context->IASetIndexBuffer(buffer.m_buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
}

void dx3d::DeviceContext::setViewportSize(const Rect& size)
{
	D3D11_VIEWPORT vp{};
	vp.Width = static_cast<f32>(size.width);
	vp.Height = static_cast<f32>(size.height);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	m_context->RSSetViewports(1, &vp);
}

void dx3d::DeviceContext::setConstantBuffer(const ConstantBuffer& buffer)
{
	auto buf = buffer.m_buffer.Get();
	m_context->VSSetConstantBuffers(0, 1, &buf);
	m_context->PSSetConstantBuffers(0, 1, &buf);
}

void dx3d::DeviceContext::updateConstantBuffer(const ConstantBuffer& buffer, const void* data)
{
	if (!data) 
	{
		DX3DLogError("Null data pointer passed to updateConstantBuffer."); 
		return;
	}

	auto buf = buffer.m_buffer.Get();
	D3D11_MAPPED_SUBRESOURCE mapped{};

	auto hr = m_context->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	if (FAILED(hr))
	{
		DX3DLogError("ID3D11DeviceContext::Map failed.");
		return;
	}

	std::memcpy(mapped.pData, data, buffer.m_size);
	m_context->Unmap(buf, 0);
}

void dx3d::DeviceContext::drawTriangleList(ui32 vertexCount, ui32 startVertexLocation)
{
	m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_context->Draw(vertexCount, startVertexLocation);
}

void dx3d::DeviceContext::drawIndexedTriangleList(ui32 indexCount, ui32 startVertexIndex, ui32 startIndexLocation)
{
	m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_context->DrawIndexed(indexCount, startIndexLocation, startVertexIndex);
}