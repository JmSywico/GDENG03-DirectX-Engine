#include <DX3D/Graphics/ShadowMap.h>

dx3d::ShadowMap::ShadowMap(
	const ShadowMapDesc& desc,
	const GraphicsResourceDesc& graphicsDesc
)
	: GraphicsResource(graphicsDesc),
	m_width(desc.width),
	m_height(desc.height),
	m_cube(desc.cube)
{
	if (
		m_width == 0 ||
		m_height == 0
		)
	{
		DX3DLogThrowError(
			"Shadow map dimensions must be greater than zero."
		);
	}

	D3D11_TEXTURE2D_DESC textureDesc{};

	textureDesc.Width = m_width;
	textureDesc.Height = m_height;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = m_cube ? 6 : 1;
	textureDesc.Format =
		DXGI_FORMAT_R32_TYPELESS;

	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;

	textureDesc.Usage =
		D3D11_USAGE_DEFAULT;

	textureDesc.BindFlags =
		D3D11_BIND_DEPTH_STENCIL |
		D3D11_BIND_SHADER_RESOURCE;
	textureDesc.MiscFlags = m_cube
		? D3D11_RESOURCE_MISC_TEXTURECUBE
		: 0;

	DX3DGraphicsLogThrowOnFail(
		m_device.CreateTexture2D(
			&textureDesc,
			nullptr,
			&m_texture
		),
		"Failed to create shadow map texture."
	);

	const ui32 faceCount = m_cube ? 6u : 1u;
	for (ui32 face = 0; face < faceCount; ++face)
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc{};
		depthViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
		if (m_cube)
		{
			depthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
			depthViewDesc.Texture2DArray.MipSlice = 0;
			depthViewDesc.Texture2DArray.FirstArraySlice = face;
			depthViewDesc.Texture2DArray.ArraySize = 1;
		}
		else
		{
			depthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			depthViewDesc.Texture2D.MipSlice = 0;
		}

		DX3DGraphicsLogThrowOnFail(
			m_device.CreateDepthStencilView(
				m_texture.Get(),
				&depthViewDesc,
				&m_depthStencilViews[face]
			),
			"Failed to create shadow map depth-stencil view."
		);
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC
		resourceViewDesc{};

	resourceViewDesc.Format =
		DXGI_FORMAT_R32_FLOAT;

	if (m_cube)
	{
		resourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		resourceViewDesc.TextureCube.MostDetailedMip = 0;
		resourceViewDesc.TextureCube.MipLevels = 1;
	}
	else
	{
		resourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		resourceViewDesc.Texture2D.MostDetailedMip = 0;
		resourceViewDesc.Texture2D.MipLevels = 1;
	}

	DX3DGraphicsLogThrowOnFail(
		m_device.CreateShaderResourceView(
			m_texture.Get(),
			&resourceViewDesc,
			&m_shaderResourceView
		),
		"Failed to create shadow map shader-resource view."
	);

	D3D11_SAMPLER_DESC samplerDesc{};

	samplerDesc.Filter =
		D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;

	samplerDesc.AddressU =
		D3D11_TEXTURE_ADDRESS_BORDER;

	samplerDesc.AddressV =
		D3D11_TEXTURE_ADDRESS_BORDER;

	samplerDesc.AddressW =
		D3D11_TEXTURE_ADDRESS_BORDER;

	samplerDesc.BorderColor[0] = 1.0f;
	samplerDesc.BorderColor[1] = 1.0f;
	samplerDesc.BorderColor[2] = 1.0f;
	samplerDesc.BorderColor[3] = 1.0f;

	samplerDesc.ComparisonFunc =
		D3D11_COMPARISON_LESS_EQUAL;

	samplerDesc.MinLOD = 0.0f;
	samplerDesc.MaxLOD =
		D3D11_FLOAT32_MAX;

	DX3DGraphicsLogThrowOnFail(
		m_device.CreateSamplerState(
			&samplerDesc,
			&m_samplerState
		),
		"Failed to create shadow map sampler."
	);
}

dx3d::ui32
dx3d::ShadowMap::getWidth() const noexcept
{
	return m_width;
}

dx3d::ui32
dx3d::ShadowMap::getHeight() const noexcept
{
	return m_height;
}

bool dx3d::ShadowMap::isCube() const noexcept
{
	return m_cube;
}
