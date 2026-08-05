#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include <fmt/format.h>

namespace Logging
{
	enum class Level : uint8_t { Trace = 0, Debug, Info, Warn, Error, Fatal };

	// Initializes debugger, console, and optional rotating-file sinks.
	bool Initialize(const std::string& filePath = "", Level level = Level::Debug);

	void Shutdown();

	void Log(Level level, const char* file, int line, const char* func, const std::string& message);

	template <typename... Args>
	void Logf(
		Level level,
		const char* file,
		int line,
		const char* func,
		fmt::format_string<Args...> format,
		Args&&... args)
	{
		Log(level, file, line, func, fmt::format(format, std::forward<Args>(args)...));
	}
}

#define LOG_TRACE(msg) Logging::Log(Logging::Level::Trace, __FILE__, __LINE__, __FUNCTION__, (msg))
#define LOG_DEBUG(msg) Logging::Log(Logging::Level::Debug, __FILE__, __LINE__, __FUNCTION__, (msg))
#define LOG_INFO(msg)  Logging::Log(Logging::Level::Info,  __FILE__, __LINE__, __FUNCTION__, (msg))
#define LOG_WARN(msg)  Logging::Log(Logging::Level::Warn,  __FILE__, __LINE__, __FUNCTION__, (msg))
#define LOG_ERROR(msg) Logging::Log(Logging::Level::Error, __FILE__, __LINE__, __FUNCTION__, (msg))
#define LOG_FATAL(msg) Logging::Log(Logging::Level::Fatal, __FILE__, __LINE__, __FUNCTION__, (msg))

#define LOG_TRACEF(format, ...) Logging::Logf(Logging::Level::Trace, __FILE__, __LINE__, __FUNCTION__, format, __VA_ARGS__)
#define LOG_DEBUGF(format, ...) Logging::Logf(Logging::Level::Debug, __FILE__, __LINE__, __FUNCTION__, format, __VA_ARGS__)
#define LOG_INFOF(format, ...)  Logging::Logf(Logging::Level::Info,  __FILE__, __LINE__, __FUNCTION__, format, __VA_ARGS__)
#define LOG_WARNF(format, ...)  Logging::Logf(Logging::Level::Warn,  __FILE__, __LINE__, __FUNCTION__, format, __VA_ARGS__)
#define LOG_ERRORF(format, ...) Logging::Logf(Logging::Level::Error, __FILE__, __LINE__, __FUNCTION__, format, __VA_ARGS__)
#define LOG_FATALF(format, ...) Logging::Logf(Logging::Level::Fatal, __FILE__, __LINE__, __FUNCTION__, format, __VA_ARGS__)
