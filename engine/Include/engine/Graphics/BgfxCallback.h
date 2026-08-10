#pragma once

#include <bgfx/bgfx.h>

#include <filesystem>

namespace enignE::Graphics
{
	class BgfxCallback final : public bgfx::CallbackI
	{
	public:
		explicit BgfxCallback(std::filesystem::path cacheDirectory);

		void fatal(
			const char* filePath,
			std::uint16_t line,
			bgfx::Fatal::Enum code,
			const char* message) override;
		void traceVargs(
			const char* filePath,
			std::uint16_t line,
			const char* format,
			va_list arguments) override;
		void profilerBegin(
			const char* name,
			std::uint32_t color,
			const char* filePath,
			std::uint16_t line) override;
		void profilerBeginLiteral(
			const char* name,
			std::uint32_t color,
			const char* filePath,
			std::uint16_t line) override;
		void profilerEnd() override;
		std::uint32_t cacheReadSize(std::uint64_t id) override;
		bool cacheRead(std::uint64_t id, void* data, std::uint32_t size) override;
		void cacheWrite(std::uint64_t id, const void* data, std::uint32_t size) override;
		void screenShot(
			const char* filePath,
			std::uint32_t width,
			std::uint32_t height,
			std::uint32_t pitch,
			bgfx::TextureFormat::Enum format,
			const void* data,
			std::uint32_t size,
			bool yFlip) override;
		void captureBegin(
			std::uint32_t width,
			std::uint32_t height,
			std::uint32_t pitch,
			bgfx::TextureFormat::Enum format,
			bool yFlip) override;
		void captureEnd() override;
		void captureFrame(const void* data, std::uint32_t size) override;

		const std::filesystem::path& GetCacheDirectory() const { return m_cacheDirectory; }

	private:
		std::filesystem::path CachePath(std::uint64_t id) const;

		std::filesystem::path m_cacheDirectory;
	};

	std::filesystem::path GetDefaultBgfxCacheDirectory();
}
