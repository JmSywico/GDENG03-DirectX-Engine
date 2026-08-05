#pragma once

#include <DirectXMath.h>

namespace enignE::Editor
{
	struct EditorContext;

	// Widgets are reusable editor controls that are not full panels.
	class GizmoToolbar
	{
	public:
		void Draw(
			EditorContext& context,
			const DirectX::XMFLOAT2& sceneViewportOrigin,
			const DirectX::XMUINT2& sceneViewportSize);
	};
}
