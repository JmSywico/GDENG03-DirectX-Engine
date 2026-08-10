#include "Editor/EditorContext.h"

#include "Scene/Scene.h"

#include <algorithm>
#include <utility>

namespace enignE::Editor
{
	EditorContext::EditorContext(
		Scene::Scene& scene,
		MatrixProvider viewProvider,
		MatrixProvider projectionProvider,
		ViewportProvider viewportProvider,
		SceneChangedCallback sceneChangedCallback,
		WindowAction minimizeWindow,
		WindowAction toggleMaximizeWindow,
		WindowAction closeWindow,
		WindowAction beginTitleBarDrag,
		WindowStateProvider isWindowMaximized)
		: ActiveScene(&scene)
		, ViewProvider(std::move(viewProvider))
		, ProjectionProvider(std::move(projectionProvider))
		, ViewportProviderCallback(std::move(viewportProvider))
		, SceneChanged(std::move(sceneChangedCallback))
		, MinimizeWindow(std::move(minimizeWindow))
		, ToggleMaximizeWindow(std::move(toggleMaximizeWindow))
		, CloseWindow(std::move(closeWindow))
		, BeginTitleBarDrag(std::move(beginTitleBarDrag))
		, IsWindowMaximized(std::move(isWindowMaximized))
	{
	}

	bool EditorContext::IsSelectedEntityValid() const
	{
		return ActiveScene
			&& SelectedEntity != entt::null
			&& ActiveScene->IsEntityValid(SelectedEntity);
	}

	bool EditorContext::IsEntitySelected(entt::entity entity) const
	{
		if (!ActiveScene || !ActiveScene->IsEntityValid(entity))
			return false;
		if (entity == SelectedEntity)
			return true;
		return std::find(SelectedEntities.begin(), SelectedEntities.end(), entity)
			!= SelectedEntities.end();
	}

	std::vector<entt::entity> EditorContext::GetSelectedEntities() const
	{
		std::vector<entt::entity> result;
		if (!ActiveScene)
			return result;
		for (const entt::entity entity : SelectedEntities)
		{
			if (ActiveScene->IsEntityValid(entity)
				&& std::find(result.begin(), result.end(), entity) == result.end())
			{
				result.push_back(entity);
			}
		}
		if (ActiveScene->IsEntityValid(SelectedEntity)
			&& std::find(result.begin(), result.end(), SelectedEntity) == result.end())
		{
			result.push_back(SelectedEntity);
		}
		return result;
	}

	void EditorContext::SelectEntity(entt::entity entity, bool additive)
	{
		if (!ActiveScene || !ActiveScene->IsEntityValid(entity))
		{
			if (!additive)
				ClearSelection();
			return;
		}
		if (!additive)
			SelectedEntities.clear();
		if (std::find(SelectedEntities.begin(), SelectedEntities.end(), entity)
			== SelectedEntities.end())
		{
			SelectedEntities.push_back(entity);
		}
		SelectedEntity = entity;
	}

	void EditorContext::ToggleEntitySelection(entt::entity entity)
	{
		if (!ActiveScene || !ActiveScene->IsEntityValid(entity))
			return;
		auto it = std::find(SelectedEntities.begin(), SelectedEntities.end(), entity);
		if (it == SelectedEntities.end())
		{
			SelectedEntities.push_back(entity);
			SelectedEntity = entity;
			return;
		}
		SelectedEntities.erase(it);
		SelectedEntity = SelectedEntities.empty() ? entt::null : SelectedEntities.back();
	}

	void EditorContext::SelectEntities(const std::vector<entt::entity>& entities)
	{
		ClearSelection();
		for (const entt::entity entity : entities)
		{
			if (ActiveScene && ActiveScene->IsEntityValid(entity)
				&& std::find(SelectedEntities.begin(), SelectedEntities.end(), entity)
					== SelectedEntities.end())
			{
				SelectedEntities.push_back(entity);
			}
		}
		SelectedEntity = SelectedEntities.empty() ? entt::null : SelectedEntities.back();
	}

	void EditorContext::ClearSelection()
	{
		SelectedEntity = entt::null;
		SelectedEntities.clear();
	}

	void EditorContext::ValidateSelection()
	{
		if (!ActiveScene)
		{
			ClearSelection();
			return;
		}
		SelectedEntities.erase(
			std::remove_if(
				SelectedEntities.begin(),
				SelectedEntities.end(),
				[this](entt::entity entity) { return !ActiveScene->IsEntityValid(entity); }),
			SelectedEntities.end());
		if (!ActiveScene->IsEntityValid(SelectedEntity))
			SelectedEntity = SelectedEntities.empty() ? entt::null : SelectedEntities.back();
		else if (std::find(SelectedEntities.begin(), SelectedEntities.end(), SelectedEntity)
			== SelectedEntities.end())
			SelectedEntities.push_back(SelectedEntity);
	}
}
