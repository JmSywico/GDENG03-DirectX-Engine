#include "Graphics/BgfxCallback.h"

#include "Logging/Logging.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace enignE::Graphics
{
	namespace
	{
		constexpr std::uintmax_t MaxCacheEntrySize = 64ull * 1024ull * 1024ull;
	}

	BgfxCallback::BgfxCallback(std::filesystem::path cacheDirectory)
		: m_cacheDirectory(std::move(cacheDirectory))
	{
		std::error_code error;
		std::filesystem::create_directories(m_cacheDirectory, error);
		if (error)
			LOG_WARNF("Unable to create bgfx pipeline cache '{}': {}", m_cacheDirectory.string(), error.message());
	}

	void BgfxCallback::fatal(
		const char* filePath,
		std::uint16_t line,
		bgfx::Fatal::Enum code,
		const char* message)
	{
		LOG_ERRORF(
			"bgfx fatal at {}:{} (code {}): {}",
			filePath ? filePath : "<unknown>",
			line,
			static_cast<int>(code),
			message ? message : "<no message>");
		if (code != bgfx::Fatal::DebugCheck)
			std::abort();
	}

	void BgfxCallback::traceVargs(
		const char* filePath,
		std::uint16_t line,
		const char* format,
		va_list arguments)
	{
		std::array<char, 2048> message{};
		std::vsnprintf(message.data(), message.size(), format, arguments);
		LOG_TRACEF("bgfx {}:{}: {}", filePath ? filePath : "<unknown>", line, message.data());
	}

	void BgfxCallback::profilerBegin(
		const char*, std::uint32_t, const char*, std::uint16_t)
	{
	}

	void BgfxCallback::profilerBeginLiteral(
		const char*, std::uint32_t, const char*, std::uint16_t)
	{
	}

	void BgfxCallback::profilerEnd()
	{
	}

	std::uint32_t BgfxCallback::cacheReadSize(std::uint64_t id)
	{
		std::error_code error;
		const std::uintmax_t size = std::filesystem::file_size(CachePath(id), error);
		if (error || size == 0 || size > MaxCacheEntrySize
			|| size > std::numeric_limits<std::uint32_t>::max())
			return 0;
		return static_cast<std::uint32_t>(size);
	}

	bool BgfxCallback::cacheRead(std::uint64_t id, void* data, std::uint32_t size)
	{
		if (!data || size == 0 || size > MaxCacheEntrySize)
			return false;
		std::ifstream stream(CachePath(id), std::ios::binary);
		return stream
			&& stream.read(static_cast<char*>(data), static_cast<std::streamsize>(size))
			&& stream.gcount() == static_cast<std::streamsize>(size);
	}

	void BgfxCallback::cacheWrite(std::uint64_t id, const void* data, std::uint32_t size)
	{
		if (!data || size == 0 || size > MaxCacheEntrySize)
			return;
		const std::filesystem::path path = CachePath(id);
		std::error_code error;
		std::filesystem::create_directories(m_cacheDirectory, error);
		if (error)
			return;
		std::filesystem::path temporaryPath = path;
		temporaryPath += ".tmp";
		std::ofstream stream(temporaryPath, std::ios::binary | std::ios::trunc);
		if (!stream.write(static_cast<const char*>(data), static_cast<std::streamsize>(size)))
		{
			stream.close();
			std::filesystem::remove(temporaryPath, error);
			return;
		}
		stream.close();
		std::filesystem::remove(path, error);
		error.clear();
		std::filesystem::rename(temporaryPath, path, error);
		if (error)
			std::filesystem::remove(temporaryPath, error);
	}

	void BgfxCallback::screenShot(
		const char*, std::uint32_t, std::uint32_t, std::uint32_t,
		bgfx::TextureFormat::Enum, const void*, std::uint32_t, bool)
	{
	}

	void BgfxCallback::captureBegin(
		std::uint32_t, std::uint32_t, std::uint32_t, bgfx::TextureFormat::Enum, bool)
	{
	}

	void BgfxCallback::captureEnd()
	{
	}

	void BgfxCallback::captureFrame(const void*, std::uint32_t)
	{
	}

	std::filesystem::path BgfxCallback::CachePath(std::uint64_t id) const
	{
		std::ostringstream name;
		name << std::hex << std::setfill('0') << std::setw(16) << id << ".bin";
		return m_cacheDirectory / name.str();
	}

	std::filesystem::path GetDefaultBgfxCacheDirectory()
	{
		#if defined(_WIN32)
		char* localAppData = nullptr;
		std::size_t localAppDataLength = 0;
		if (_dupenv_s(&localAppData, &localAppDataLength, "LOCALAPPDATA") == 0
			&& localAppData && *localAppData)
		{
			const std::filesystem::path result = std::filesystem::path(localAppData)
				/ "enignE" / "cache" / "bgfx" / "d3d12-v1";
			std::free(localAppData);
			return result;
		}
		std::free(localAppData);
		#else
		if (const char* localAppData = std::getenv("LOCALAPPDATA"); localAppData && *localAppData)
			return std::filesystem::path(localAppData) / "enignE" / "cache" / "bgfx" / "d3d12-v1";
		#endif
		std::error_code error;
		const std::filesystem::path temporary = std::filesystem::temp_directory_path(error);
		return (error ? std::filesystem::current_path() : temporary)
			/ "enignE" / "cache" / "bgfx" / "d3d12-v1";
	}
}
