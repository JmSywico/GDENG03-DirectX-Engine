#pragma once

#include <string>

namespace Logging
{
	// Ensure a console is present (allocate + redirect std streams on Windows).
	// Returns true if a console is available after the call.
	bool PlatformConsoleEnsure();

	// Write text to the console if available (no newline required).
	void PlatformConsoleWrite(const std::string& text);

	// Free console / undo allocations done by PlatformConsoleEnsure where appropriate.
	void PlatformConsoleShutdown();

	// Platform-specific debugger output (OutputDebugString on Windows).
	void PlatformOutputDebugString(const std::string& text);
}
