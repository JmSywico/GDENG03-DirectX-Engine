#pragma once

#include <Windows.h>

namespace enignE::Platform
{
	inline void EnablePerMonitorDpiAwareness()
	{
		// Must run before creating any engine or common-dialog windows.
		SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	}
}
