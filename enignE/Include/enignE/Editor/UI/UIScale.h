#pragma once

#include <algorithm>
#include <imgui.h>

namespace enignE::Editor::UI
{
	inline float g_scaleFactor = 1.0f;
	inline void SetScaleFactor(float value)
	{
		g_scaleFactor = std::clamp(value, 0.75f, 3.0f);
	}
	inline float ScaleFactor() { return g_scaleFactor; }
	inline float Scale(float value) { return value * ScaleFactor(); }
	inline ImVec2 Scale(float x, float y) { return {Scale(x), Scale(y)}; }
}
