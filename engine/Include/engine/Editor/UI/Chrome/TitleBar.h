#pragma once

namespace enignE::Editor
{
	struct EditorContext;

	// Chrome owns application/window UI rather than scene editing UI.
	class TitleBar
	{
	public:
		void Draw(EditorContext& context);
	};
}
