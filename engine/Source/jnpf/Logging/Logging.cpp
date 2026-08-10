#include "Logging/Logging.h"
#include "Logging/LoggingPlatform.h"

#include <memory>
#include <vector>

#include <spdlog/logger.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#if defined(_WIN32)
#include <spdlog/sinks/msvc_sink.h>
#endif
#include <spdlog/spdlog.h>

namespace Logging
{
	namespace
	{
		std::shared_ptr<spdlog::logger> g_logger;

		spdlog::level::level_enum ToSpdlogLevel(Level level)
		{
			switch (level)
			{
			case Level::Trace: return spdlog::level::trace;
			case Level::Debug: return spdlog::level::debug;
			case Level::Info: return spdlog::level::info;
			case Level::Warn: return spdlog::level::warn;
			case Level::Error: return spdlog::level::err;
			case Level::Fatal: return spdlog::level::critical;
			default: return spdlog::level::off;
			}
		}
	}

	bool Initialize(const std::string& filePath, Level level)
	{
		if (g_logger)
			return true;

		PlatformConsoleEnsure();

		try
		{
			std::vector<spdlog::sink_ptr> sinks;
			sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
#if defined(_WIN32)
			sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#endif
			bool fileSinkReady = filePath.empty();
			if (!filePath.empty())
			{
				try
				{
					sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
						filePath,
						5 * 1024 * 1024,
						3));
					fileSinkReady = true;
				}
				catch (const spdlog::spdlog_ex& error)
				{
					PlatformOutputDebugString(fmt::format(
						"File logging initialization failed for '{}': {}\n",
						filePath,
						error.what()));
				}
			}

			g_logger = std::make_shared<spdlog::logger>("jnpf", sinks.begin(), sinks.end());
			g_logger->set_level(ToSpdlogLevel(level));
			g_logger->set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] [%s:%#] %!(): %v");
			g_logger->flush_on(spdlog::level::err);
			spdlog::register_logger(g_logger);
			return fileSinkReady;
		}
		catch (const spdlog::spdlog_ex& error)
		{
			PlatformOutputDebugString(fmt::format("Logging initialization failed: {}\n", error.what()));
			g_logger.reset();
			return false;
		}
	}

	void Shutdown()
	{
		if (g_logger)
		{
			g_logger->flush();
			spdlog::drop(g_logger->name());
			g_logger.reset();
		}

		PlatformConsoleShutdown();
	}

	void Log(Level level, const char* file, int line, const char* func, const std::string& message)
	{
		if (!g_logger)
			return;

		g_logger->log(
			spdlog::source_loc{file, line, func},
			ToSpdlogLevel(level),
			message);
	}
}
