#pragma once

namespace jnpf::Editor
{
	struct ViewportRenderPlan
	{
		bool RenderScene = false;
		bool RenderGame = false;
		bool ClearGame = false;
		bool SceneOwnsShadows = false;
		bool GameOwnsShadows = false;

		static ViewportRenderPlan Build(
			bool sceneVisible,
			bool gameVisible,
			bool hasGameCamera,
			bool preferGameShadows)
		{
			ViewportRenderPlan plan;
			plan.RenderScene = sceneVisible;
			plan.RenderGame = gameVisible && hasGameCamera;
			plan.ClearGame = gameVisible && !hasGameCamera;
			plan.GameOwnsShadows = plan.RenderGame
				&& (!plan.RenderScene || preferGameShadows);
			plan.SceneOwnsShadows = plan.RenderScene && !plan.GameOwnsShadows;
			return plan;
		}
	};
}
