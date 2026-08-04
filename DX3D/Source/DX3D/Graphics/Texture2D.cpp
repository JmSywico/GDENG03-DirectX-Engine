#include <DX3D/Graphics/Texture2D.h>

#include <wincodec.h>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")

dx3d::Texture2D::Texture2D(
	const Texture2DDesc& desc,
	const GraphicsResourceDesc& graphicsDesc
)
	: GraphicsResource(graphicsDesc)
{
	if (!desc.filePath)
	{
		DX3DLogThrowInvalidArg(
			"No texture file path provided."
		);
	}

	struct ComScope
	{
		bool shouldUninitialize{ false };

		~ComScope()
		{
			if (shouldUninitialize)
			{
				CoUninitialize();
			}
		}
	};

	ComScope comScope{};

	const HRESULT comResult =
		CoInitializeEx(
			nullptr,
			COINIT_MULTITHREADED
		);

	if (SUCCEEDED(comResult))
	{
		comScope.shouldUninitialize = true;
	}
	else if (comResult != RPC_E_CHANGED_MODE)
	{
		DX3DLogThrowError(
			"Failed to initialize COM for texture loading."
		);
	}

	Microsoft::WRL::ComPtr<
		IWICImagingFactory
	> imagingFactory{};

	DX3DGraphicsLogThrowOnFail(
		CoCreateInstance(
			CLSID_WICImagingFactory,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(
				imagingFactory.GetAddressOf()
			)
		),
		"Failed to create the WIC imaging factory."
	);

	Microsoft::WRL::ComPtr<
		IWICBitmapDecoder
	> decoder{};

	DX3DGraphicsLogThrowOnFail(
		imagingFactory->CreateDecoderFromFilename(
			desc.filePath,
			nullptr,
			GENERIC_READ,
			WICDecodeMetadataCacheOnLoad,
			decoder.GetAddressOf()
		),
		"Failed to open the texture file."
	);

	Microsoft::WRL::ComPtr<
		IWICBitmapFrameDecode
	> frame{};

	DX3DGraphicsLogThrowOnFail(
		decoder->GetFrame(
			0,
			frame.GetAddressOf()
		),
		"Failed to read the texture frame."
	);

	UINT width{};
	UINT height{};

	DX3DGraphicsLogThrowOnFail(
		frame->GetSize(
			&width,
			&height
		),
		"Failed to read the texture dimensions."
	);

	if (width == 0 ||
		height == 0)
	{
		DX3DLogThrowError(
			"Texture dimensions must be greater than zero."
		);
	}

	Microsoft::WRL::ComPtr<
		IWICFormatConverter
	> converter{};

	DX3DGraphicsLogThrowOnFail(
		imagingFactory->CreateFormatConverter(
			converter.GetAddressOf()
		),
		"Failed to create the texture format converter."
	);

	DX3DGraphicsLogThrowOnFail(
		converter->Initialize(
			frame.Get(),
			GUID_WICPixelFormat32bppRGBA,
			WICBitmapDitherTypeNone,
			nullptr,
			0.0,
			WICBitmapPaletteTypeCustom
		),
		"Failed to convert the texture to RGBA."
	);

	const UINT bytesPerPixel = 4;
	const UINT rowPitch =
		width * bytesPerPixel;

	std::vector<unsigned char> pixels(
		static_cast<size_t>(rowPitch) *
		static_cast<size_t>(height)
	);

	DX3DGraphicsLogThrowOnFail(
		converter->CopyPixels(
			nullptr,
			rowPitch,
			static_cast<UINT>(
				pixels.size()
				),
			pixels.data()
		),
		"Failed to copy the texture pixels."
	);

	D3D11_TEXTURE2D_DESC textureDesc{};

	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;

	textureDesc.Format =
		DXGI_FORMAT_R8G8B8A8_UNORM;

	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;

	textureDesc.Usage =
		D3D11_USAGE_DEFAULT;

	textureDesc.BindFlags =
		D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA textureData{};

	textureData.pSysMem =
		pixels.data();

	textureData.SysMemPitch =
		rowPitch;

	DX3DGraphicsLogThrowOnFail(
		m_device.CreateTexture2D(
			&textureDesc,
			&textureData,
			m_texture.GetAddressOf()
		),
		"Failed to create the DirectX texture."
	);

	D3D11_SHADER_RESOURCE_VIEW_DESC
		resourceViewDesc{};

	resourceViewDesc.Format =
		textureDesc.Format;

	resourceViewDesc.ViewDimension =
		D3D11_SRV_DIMENSION_TEXTURE2D;

	resourceViewDesc.Texture2D.MostDetailedMip = 0;
	resourceViewDesc.Texture2D.MipLevels = 1;

	DX3DGraphicsLogThrowOnFail(
		m_device.CreateShaderResourceView(
			m_texture.Get(),
			&resourceViewDesc,
			m_shaderResourceView.GetAddressOf()
		),
		"Failed to create the texture shader-resource view."
	);

	D3D11_SAMPLER_DESC samplerDesc{};

	samplerDesc.Filter =
		D3D11_FILTER_MIN_MAG_MIP_LINEAR;

	samplerDesc.AddressU =
		D3D11_TEXTURE_ADDRESS_WRAP;

	samplerDesc.AddressV =
		D3D11_TEXTURE_ADDRESS_WRAP;

	samplerDesc.AddressW =
		D3D11_TEXTURE_ADDRESS_WRAP;

	samplerDesc.ComparisonFunc =
		D3D11_COMPARISON_NEVER;

	samplerDesc.MinLOD = 0.0f;
	samplerDesc.MaxLOD =
		D3D11_FLOAT32_MAX;

	DX3DGraphicsLogThrowOnFail(
		m_device.CreateSamplerState(
			&samplerDesc,
			m_samplerState.GetAddressOf()
		),
		"Failed to create the texture sampler."
	);

	m_width =
		static_cast<ui32>(width);

	m_height =
		static_cast<ui32>(height);
}

dx3d::ui32
dx3d::Texture2D::getWidth() const noexcept
{
	return m_width;
}

dx3d::ui32
dx3d::Texture2D::getHeight() const noexcept
{
	return m_height;
}