#include "Logging/LoggingPlatform.h"

#if !defined(_WIN32)

namespace Logging
{
	bool PlatformConsoleEnsure() { return false; }

	void PlatformConsoleWrite(const std::string&)
	{
	}

	void PlatformConsoleShutdown()
	{
	}

	void PlatformOutputDebugString(const std::string&)
	{
	}
}

#endif
