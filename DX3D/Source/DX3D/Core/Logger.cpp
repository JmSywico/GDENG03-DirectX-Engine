#include <DX3D/Core/Logger.h>
#include <iostream>

dx3d::Logger::Logger(LogLevel logLevel): m_logLevel(logLevel)
{
}

dx3d::Logger::~Logger()
{
}

void dx3d::Logger::_log(LogLevel level, const char* message)
{
	auto logLevelToString = [](LogLevel level) {
		switch (level)
		{
		case LogLevel::Info: return "Info";
		case LogLevel::Warning: return "Warning";
		case LogLevel::Error: return "Error";
		default: return "Unknown";
		}
	};

	{
		std::scoped_lock lock(m_entriesMutex);
		m_entries.push_back({ level, message ? message : "" });
		constexpr size_t maximumEntries = 1000;
		if (m_entries.size() > maximumEntries) m_entries.pop_front();
	}
	if (level > m_logLevel) return;
	std::clog << "[DX3D " << logLevelToString(level) << "]: " << message << "\n";
}

std::vector<dx3d::Logger::Entry> dx3d::Logger::getEntries() const
{
	std::scoped_lock lock(m_entriesMutex);
	return { m_entries.begin(), m_entries.end() };
}

void dx3d::Logger::clear()
{
	std::scoped_lock lock(m_entriesMutex);
	m_entries.clear();
}
