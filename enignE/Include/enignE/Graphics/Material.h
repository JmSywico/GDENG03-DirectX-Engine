#pragma once

#include <DirectXMath.h>

#include <cstdint>
#include <memory>
#include <string>

class Texture2D;

enum class MaterialMode : uint32_t
{
	LitTint = 0,
	Rainbow = 1,
	FlatGreen = 2,
	FlatRed = 3,
	FlatBlue = 4,
};

struct MaterialResource
{
	DirectX::XMFLOAT4 Albedo = {1.0f, 1.0f, 1.0f, 1.0f};
	DirectX::XMFLOAT3 Emissive = {0.0f, 0.0f, 0.0f};
	float Metallic = 0.0f;
	float Roughness = 1.0f;
	MaterialMode Mode = MaterialMode::LitTint;
	std::shared_ptr<Texture2D> AlbedoTexture;
	std::uint64_t AlbedoTextureHandle = 0;
	std::string AlbedoTexturePath;
	std::shared_ptr<Texture2D> NormalTexture;
	std::uint64_t NormalTextureHandle = 0;
	std::string NormalTexturePath;
	std::shared_ptr<Texture2D> MetallicRoughnessTexture;
	std::uint64_t MetallicRoughnessTextureHandle = 0;
	std::string MetallicRoughnessTexturePath;
};
