#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct PreparedTexture2D
{
	std::string SourcePath;
	std::vector<std::uint8_t> RGBA8;
	std::uint16_t Width = 0;
	std::uint16_t Height = 0;
};

class Texture2D
{
public:
	Texture2D() = default;
	~Texture2D() = default;
	Texture2D(const Texture2D&) = delete;
	Texture2D& operator=(const Texture2D&) = delete;

	static std::shared_ptr<Texture2D> LoadFromFile(const std::string& filepath);
	static std::optional<PreparedTexture2D> PrepareFromFile(const std::string& filepath);
	static std::shared_ptr<Texture2D> Finalize(PreparedTexture2D prepared);
	static std::shared_ptr<Texture2D> CreateSolidColor(
		std::uint32_t rgba, const char* debugName = "SolidColor");

	ID3D11ShaderResourceView* GetSRV() const { return m_srv.Get(); }
	ID3D11SamplerState* GetSampler() const { return m_sampler.Get(); }
	std::uint16_t GetWidth() const { return m_width; }
	std::uint16_t GetHeight() const { return m_height; }
	bool IsValid() const { return m_texture && m_srv && m_sampler; }

private:
	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_texture;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_srv;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler;
	std::uint16_t m_width = 0;
	std::uint16_t m_height = 0;
};
