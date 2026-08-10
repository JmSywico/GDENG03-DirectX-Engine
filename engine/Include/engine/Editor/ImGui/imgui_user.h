#include <stdint.h>

namespace ImGui
{
	struct Font
	{
		enum Enum
		{
			Regular,
			Mono,

			Count
		};
	};

	void PushFont(Font::Enum font, float fontSizeBaseUnscaled = 0.0f);

	void InitDockContext();
	void ShutdownDockContext();
}

#include "widgets/gizmo.h"
