#include "Logging/LoggingPlatform.h"

#if defined(_WIN32)
#include <Windows.h>
#include <cstdio>
#include <iostream>

namespace Logging
{
	static bool g_consoleAllocated_local = false;

	bool PlatformConsoleEnsure()
	{
		if (g_consoleAllocated_local) return true;

		// If there's already a console (launched from terminal), mark as available.
		if (GetConsoleWindow())
		{
			g_consoleAllocated_local = true;
			return true;
		}

		if (!AllocConsole())
			return false;

		FILE* conin = nullptr;
		FILE* conout = nullptr;
		freopen_s(&conin, "CONIN$", "r", stdin);
		freopen_s(&conout, "CONOUT$", "w", stdout);
		freopen_s(&conout, "CONOUT$", "w", stderr);

		std::ios::sync_with_stdio(true);
		std::cout.clear();
		std::cerr.clear();

		g_consoleAllocated_local = true;
		return true;
	}

	void PlatformConsoleWrite(const std::string& text)
	{
		if (!g_consoleAllocated_local) return;
		std::cout << text;
		std::cout.flush();
	}

	void PlatformConsoleShutdown()
	{
		if (!g_consoleAllocated_local) return;
		std::cout.flush();
		std::cerr.flush();
		FreeConsole();
		g_consoleAllocated_local = false;
	}

	void PlatformOutputDebugString(const std::string& text)
	{
		OutputDebugStringA(text.c_str());
	}
}
#endif
