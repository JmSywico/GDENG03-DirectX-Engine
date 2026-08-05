#include "Graphics/Texture2D.h"

#include "Graphics/DX11/DX11Context.h"
#include "Logging/Logging.h"

#include <wincodec.h>

#include <filesystem>
#include <limits>

namespace
{
	Microsoft::WRL::ComPtr<IWICImagingFactory> CreateWicFactory()
	{
		const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) return {};
		Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
		if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
			CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) return {};
		return factory;
	}
}

std::shared_ptr<Texture2D> Texture2D::LoadFromFile(const std::string& filepath)
{
	auto prepared = PrepareFromFile(filepath);
	return prepared ? Finalize(std::move(*prepared)) : nullptr;
}

std::optional<PreparedTexture2D> Texture2D::PrepareFromFile(const std::string& filepath)
{
	auto factory = CreateWicFactory();
	if (!factory)
	{
		LOG_ERRORF("Texture2D::PrepareFromFile - WIC unavailable for '{}'", filepath);
		return std::nullopt;
	}
	Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
	const std::wstring path = std::filesystem::path(filepath).wstring();
	if (FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
		WICDecodeMetadataCacheOnDemand, &decoder)))
	{
		LOG_ERRORF("Texture2D::PrepareFromFile - unable to decode '{}'", filepath);
		return std::nullopt;
	}
	Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
	Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
	UINT width = 0, height = 0;
	if (FAILED(decoder->GetFrame(0, &frame)) || FAILED(frame->GetSize(&width, &height)) ||
		width == 0 || height == 0 || width > std::numeric_limits<std::uint16_t>::max() ||
		height > std::numeric_limits<std::uint16_t>::max() ||
		FAILED(factory->CreateFormatConverter(&converter)) ||
		FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
			WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
	{
		LOG_ERRORF("Texture2D::PrepareFromFile - unsupported image '{}'", filepath);
		return std::nullopt;
	}
	const std::uint64_t byteCount = static_cast<std::uint64_t>(width) * height * 4u;
	if (byteCount > std::numeric_limits<UINT>::max()) return std::nullopt;
	PreparedTexture2D prepared;
	prepared.SourcePath = filepath;
	prepared.Width = static_cast<std::uint16_t>(width);
	prepared.Height = static_cast<std::uint16_t>(height);
	prepared.RGBA8.resize(static_cast<std::size_t>(byteCount));
	if (FAILED(converter->CopyPixels(nullptr, width * 4u,
		static_cast<UINT>(byteCount), prepared.RGBA8.data()))) return std::nullopt;
	return prepared;
}

std::shared_ptr<Texture2D> Texture2D::Finalize(PreparedTexture2D prepared)
{
	auto* graphics = enignE::Graphics::DX11::Context::GetActive();
	const std::size_t expected = static_cast<std::size_t>(prepared.Width) * prepared.Height * 4u;
	if (!graphics || prepared.Width == 0 || prepared.Height == 0 || prepared.RGBA8.size() != expected)
	{
		LOG_ERRORF("Texture2D::Finalize - invalid DX11 texture data for '{}'", prepared.SourcePath);
		return nullptr;
	}
	auto texture = std::shared_ptr<Texture2D>(new Texture2D());
	D3D11_TEXTURE2D_DESC description{};
	description.Width = prepared.Width;
	description.Height = prepared.Height;
	description.MipLevels = 1;
	description.ArraySize = 1;
	description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	description.SampleDesc.Count = 1;
	description.Usage = D3D11_USAGE_IMMUTABLE;
	description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SUBRESOURCE_DATA initial{ prepared.RGBA8.data(), prepared.Width * 4u, 0 };
	if (FAILED(graphics->GetDevice().CreateTexture2D(&description, &initial, &texture->m_texture)) ||
		FAILED(graphics->GetDevice().CreateShaderResourceView(texture->m_texture.Get(), nullptr, &texture->m_srv)))
		return nullptr;
	D3D11_SAMPLER_DESC sampler{};
	sampler.Filter = D3D11_FILTER_ANISOTROPIC;
	sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler.MaxAnisotropy = 8;
	sampler.MaxLOD = D3D11_FLOAT32_MAX;
	if (FAILED(graphics->GetDevice().CreateSamplerState(&sampler, &texture->m_sampler))) return nullptr;
	texture->m_width = prepared.Width;
	texture->m_height = prepared.Height;
	LOG_INFOF("Loaded texture '{}' ({}x{})", prepared.SourcePath, prepared.Width, prepared.Height);
	return texture;
}

std::shared_ptr<Texture2D> Texture2D::CreateSolidColor(std::uint32_t rgba, const char* debugName)
{
	PreparedTexture2D prepared;
	prepared.SourcePath = debugName ? debugName : "SolidColor";
	prepared.Width = prepared.Height = 1;
	prepared.RGBA8 = { static_cast<std::uint8_t>(rgba & 0xffu),
		static_cast<std::uint8_t>((rgba >> 8u) & 0xffu),
		static_cast<std::uint8_t>((rgba >> 16u) & 0xffu),
		static_cast<std::uint8_t>((rgba >> 24u) & 0xffu) };
	return Finalize(std::move(prepared));
}
