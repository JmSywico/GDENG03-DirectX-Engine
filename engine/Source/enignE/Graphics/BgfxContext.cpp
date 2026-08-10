#include "Graphics/BgfxContext.h"

#include <atomic>

namespace enignE::Graphics
{
	namespace
	{
		std::atomic_bool g_bgfxInitialized = false;
	}

	void SetBgfxInitialized(bool initialized)
	{
		g_bgfxInitialized.store(initialized, std::memory_order_release);
	}

	bool IsBgfxInitialized()
	{
		return g_bgfxInitialized.load(std::memory_order_acquire);
	}
}
