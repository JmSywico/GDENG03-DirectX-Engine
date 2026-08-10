#include "Editor/UI/Panels/SceneHierarchyPanel.h"

#include "Editor/Commands/CommandStack.h"
#include "Editor/Commands/EditorCommand.h"
#include "Editor/EditorContext.h"
#include "Editor/UI/UIScale.h"
#include "Editor/UI/Panels/AssetBrowserPanel.h"
#include "Scene/Components/Components.h"
#include "Scene/Scene.h"

#include <imgui.h>
#include <fmt/format.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace
{
	const char* EntityName(const enignE::Scene::Scene& scene, entt::entity entity, std::string& fallback)
	{
		if (const auto* tag = scene.GetComponent<enignE::Scene::TagComponent>(entity);
			tag && tag->Tag[0] != '\0')
			return tag->Tag;
		fallback = fmt::format("Element {}", scene.GetEntityID(entity));
		return fallback.c_str();
	}

	void Reparent(
		enignE::Editor::EditorContext& context,
		entt::entity child,
		entt::entity parent)
	{
		auto& scene = *context.ActiveScene;
		if (!context.Commands || !scene.CanSetParent(child, parent))
			return;
		context.Commands->Execute(std::make_unique<enignE::Editor::SetParentCommand>(
			scene,
			scene.GetEntityID(child),
			scene.GetEntityID(scene.GetParent(child)),
			scene.GetEntityID(parent)));
	}

	const char* PrimitiveName(enignE::Scene::PrimitiveType type)
	{
		switch (type)
		{
		case enignE::Scene::PrimitiveType::Cube: return "Cube";
		case enignE::Scene::PrimitiveType::Rectangle: return "Rectangle";
		case enignE::Scene::PrimitiveType::Pyramid: return "Pyramid";
		case enignE::Scene::PrimitiveType::Plane: return "Plane";
		case enignE::Scene::PrimitiveType::Sphere: return "Sphere";
		default: return "Primitive";
		}
	}

	void DrawCreateMenu(enignE::Editor::EditorContext& context, entt::entity parent)
	{
		if (!context.Commands || !context.ActiveScene)
			return;

		auto& scene = *context.ActiveScene;
		const std::uint64_t parentID = parent == entt::null ? 0 : scene.GetEntityID(parent);
		const auto selectCreated = [&scene, &context](std::uint64_t id)
		{
			context.SelectEntity(scene.FindEntityByID(id));
		};
		const auto createEntity = [&](std::unique_ptr<enignE::Editor::CreateEntityCommand> command)
		{
			enignE::Editor::CreateEntityCommand* created = command.get();
			context.Commands->Execute(std::move(command));
			selectCreated(created->GetEntityID());
			return created->GetEntityID();
		};
		const auto createPrimitive = [&](enignE::Scene::PrimitiveType type)
		{
			if (!context.CreatePrimitiveModel)
				return;
			enignE::Scene::PrimitiveDesc desc;
			desc.Type = type;
			desc.Name = PrimitiveName(type);
			if (type == enignE::Scene::PrimitiveType::Rectangle)
			{
				desc.Width = 2.0f;
				desc.Height = 1.0f;
				desc.Depth = 1.0f;
			}
			else if (type == enignE::Scene::PrimitiveType::Plane)
			{
				desc.Width = 8.0f;
				desc.Depth = 8.0f;
			}
			auto command = std::make_unique<enignE::Editor::CreatePrimitiveCommand>(
				scene,
				desc,
				context.CreatePrimitiveModel,
				parentID);
			enignE::Editor::CreatePrimitiveCommand* created = command.get();
			context.Commands->Execute(std::move(command));
			selectCreated(created->GetEntityID());
		};

		if (ImGui::MenuItem("Empty Element"))
			createEntity(std::make_unique<enignE::Editor::CreateEntityCommand>(
				scene,
				"Element",
				parentID));
		if (ImGui::MenuItem("Camera"))
		{
			const std::uint64_t id = createEntity(std::make_unique<enignE::Editor::CreateEntityCommand>(
				scene,
				"Camera",
				enignE::Scene::CameraComponent{},
				parentID));
			if (scene.GetSettings().ActiveCameraEntityID == 0)
				context.Commands->Execute(std::make_unique<enignE::Editor::SetActiveCameraCommand>(
					scene,
					0,
					id));
		}
		if (ImGui::BeginMenu("Light"))
		{
			if (ImGui::MenuItem("Directional Light"))
			{
				const std::uint64_t id = createEntity(std::make_unique<enignE::Editor::CreateEntityCommand>(
					scene,
					"Directional Light",
					enignE::Scene::LightComponent{},
					parentID));
				if (scene.GetSettings().ActiveLightEntityID == 0)
					context.Commands->Execute(std::make_unique<enignE::Editor::SetActiveLightCommand>(
						scene,
						0,
						id));
			}
			if (ImGui::MenuItem("Point Light"))
			{
				enignE::Scene::LightComponent light;
				light.LightType = enignE::Scene::LightComponent::Type::Point;
				const std::uint64_t id = createEntity(std::make_unique<enignE::Editor::CreateEntityCommand>(
					scene,
					"Point Light",
					light,
					parentID));
				if (scene.GetSettings().ActiveLightEntityID == 0)
					context.Commands->Execute(std::make_unique<enignE::Editor::SetActiveLightCommand>(
						scene,
						0,
						id));
			}
			if (ImGui::MenuItem("Spot Light"))
			{
				enignE::Scene::LightComponent light;
				light.LightType = enignE::Scene::LightComponent::Type::Spot;
				const std::uint64_t id = createEntity(std::make_unique<enignE::Editor::CreateEntityCommand>(
					scene,
					"Spot Light",
					light,
					parentID));
				if (scene.GetSettings().ActiveLightEntityID == 0)
					context.Commands->Execute(std::make_unique<enignE::Editor::SetActiveLightCommand>(
						scene,
						0,
						id));
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("3D Object", context.CreatePrimitiveModel != nullptr))
		{
			if (ImGui::MenuItem("Cube"))
				createPrimitive(enignE::Scene::PrimitiveType::Cube);
			if (ImGui::MenuItem("Rectangle"))
				createPrimitive(enignE::Scene::PrimitiveType::Rectangle);
			if (ImGui::MenuItem("Pyramid"))
				createPrimitive(enignE::Scene::PrimitiveType::Pyramid);
			if (ImGui::MenuItem("Plane"))
				createPrimitive(enignE::Scene::PrimitiveType::Plane);
			if (ImGui::MenuItem("Sphere"))
				createPrimitive(enignE::Scene::PrimitiveType::Sphere);
			ImGui::EndMenu();
		}
	}

	std::vector<entt::entity> TopLevelSelection(enignE::Editor::EditorContext& context)
	{
		std::vector<entt::entity> roots;
		for (const entt::entity candidate : context.GetSelectedEntities())
		{
			bool hasSelectedAncestor = false;
			for (entt::entity parent = context.ActiveScene->GetParent(candidate);
				parent != entt::null;
				parent = context.ActiveScene->GetParent(parent))
			{
				if (context.IsEntitySelected(parent))
				{
					hasSelectedAncestor = true;
					break;
				}
			}
			if (!hasSelectedAncestor)
				roots.push_back(candidate);
		}
		return roots;
	}

	void DuplicateSelection(enignE::Editor::EditorContext& context)
	{
		if (!context.Commands)
			return;
		auto command = std::make_unique<enignE::Editor::CompositeCommand>();
		std::vector<enignE::Editor::DuplicateEntityCommand*> duplicates;
		for (const entt::entity entity : TopLevelSelection(context))
		{
			auto duplicate = std::make_unique<enignE::Editor::DuplicateEntityCommand>(
				*context.ActiveScene,
				entity);
			duplicates.push_back(duplicate.get());
			command->Add(std::move(duplicate));
		}
		if (command->Empty())
			return;
		context.Commands->Execute(std::move(command));
		std::vector<entt::entity> selection;
		for (const auto* duplicate : duplicates)
			selection.push_back(context.ActiveScene->FindEntityByID(duplicate->GetEntityID()));
		context.SelectEntities(selection);
	}

	void DeleteSelection(enignE::Editor::EditorContext& context)
	{
		if (!context.Commands)
			return;
		auto command = std::make_unique<enignE::Editor::CompositeCommand>();
		for (const entt::entity entity : TopLevelSelection(context))
			command->Add(std::make_unique<enignE::Editor::DeleteEntityCommand>(
				*context.ActiveScene,
				entity));
		if (!command->Empty())
			context.Commands->Execute(std::move(command));
		context.ClearSelection();
	}

	void DrawEntity(
		enignE::Editor::EditorContext& context,
		entt::entity entity,
		const std::vector<entt::entity>& traversal,
		entt::entity& rangeAnchor)
	{
		auto& scene = *context.ActiveScene;
		if (scene.HasComponent<enignE::Scene::TransientEditorComponent>(entity)) return;
		std::string fallback;
		const char* name = EntityName(scene, entity, fallback);
		const auto children = scene.GetChildren(entity);
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
			| ImGuiTreeNodeFlags_SpanAvailWidth;
		if (children.empty())
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		if (context.IsEntitySelected(entity))
			flags |= ImGuiTreeNodeFlags_Selected;

		ImGui::PushID(static_cast<int>(entt::to_integral(entity)));
		const bool open = ImGui::TreeNodeEx(name, flags);
		const bool clicked = ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen();
		if (clicked)
		{
			const ImGuiIO& io = ImGui::GetIO();
			if (io.KeyShift && rangeAnchor != entt::null)
			{
				const auto first = std::find(traversal.begin(), traversal.end(), rangeAnchor);
				const auto last = std::find(traversal.begin(), traversal.end(), entity);
				if (first != traversal.end() && last != traversal.end())
				{
					const auto low = std::min(first, last);
					const auto high = std::max(first, last);
					std::vector<entt::entity> range(low, high + 1);
					if (io.KeyCtrl)
					{
						auto selection = context.GetSelectedEntities();
						selection.insert(selection.end(), range.begin(), range.end());
						context.SelectEntities(selection);
					}
					else
						context.SelectEntities(range);
				}
			}
			else if (io.KeyCtrl)
				context.ToggleEntitySelection(entity);
			else
				context.SelectEntity(entity);
			rangeAnchor = entity;
		}
		if (ImGui::BeginDragDropSource())
		{
			const std::uint64_t id = scene.GetEntityID(entity);
			ImGui::SetDragDropPayload("ENIGNE_ENTITY_ID", &id, sizeof(id));
			ImGui::TextUnformatted(name);
			ImGui::EndDragDropSource();
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENIGNE_ENTITY_ID"))
			{
				const std::uint64_t childID = *static_cast<const std::uint64_t*>(payload->Data);
				Reparent(context, scene.FindEntityByID(childID), entity);
			}
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
				enignE::Editor::AssetDragPayloadID))
			{
				const auto& asset = *static_cast<const enignE::Editor::AssetDragPayload*>(payload->Data);
				if (asset.Type == enignE::Graphics::AssetType::Prefab && context.InstantiatePrefab)
					context.InstantiatePrefab(
						asset.Handle, asset.Path.data(), scene.GetEntityID(entity), nullptr);
				else if (asset.Type == enignE::Graphics::AssetType::Model && context.InstantiateModel)
					context.InstantiateModel(
						asset.Handle, asset.Path.data(), scene.GetEntityID(entity), nullptr);
			}
			ImGui::EndDragDropTarget();
		}
		if (ImGui::BeginPopupContextItem())
		{
			if (!context.IsEntitySelected(entity))
				context.SelectEntity(entity);
			DrawCreateMenu(context, entity);
			ImGui::Separator();
			if (ImGui::MenuItem(
				context.GetSelectedEntities().size() > 1 ? "Duplicate Selected" : "Duplicate",
				"Ctrl+D"))
			{
				DuplicateSelection(context);
			}
			if (scene.GetParent(entity) != entt::null && ImGui::MenuItem("Move to Root"))
				Reparent(context, entity, entt::null);
			if (context.GetSelectedEntities().size() == 1 && context.SaveAsPrefab
				&& ImGui::MenuItem("Save as Prefab"))
				context.SaveAsPrefab(entity);
			if (ImGui::MenuItem(
				context.GetSelectedEntities().size() > 1 ? "Delete Selected" : "Delete"))
				DeleteSelection(context);
			ImGui::EndPopup();
		}
		if (open && !children.empty())
		{
			for (const entt::entity child : children)
				DrawEntity(context, child, traversal, rangeAnchor);
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
}

namespace enignE::Editor
{
	void SceneHierarchyPanel::Draw(EditorContext& context)
	{
		if (!context.ActiveScene)
			return;
		Scene::Scene& scene = *context.ActiveScene;
		const DirectX::XMUINT2 viewport = context.ViewportProviderCallback();
		ImGui::SetNextWindowPos(UI::Scale(12.0f, 92.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(
			ImVec2(UI::Scale(300.0f), std::max(UI::Scale(280.0f), static_cast<float>(viewport.y) - UI::Scale(104.0f))),
			ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Elements"))
		{
			ImGui::End();
			return;
		}

		ImGui::Text("%s", scene.GetName().c_str());
		ImGui::SameLine();
		ImGui::TextDisabled("%zu elements", scene.GetEntityCount());
		ImGui::Separator();
		const std::vector<entt::entity> roots = scene.GetRootEntities();
		std::vector<entt::entity> traversal;
		const bool flatHierarchy = roots.size() == scene.GetEntityCount();
		if (flatHierarchy)
		{
			traversal.reserve(roots.size());
			for (const entt::entity root : roots)
			{
				if (!scene.HasComponent<Scene::TransientEditorComponent>(root))
					traversal.push_back(root);
			}
			ImGuiListClipper clipper;
			clipper.Begin(static_cast<int>(traversal.size()));
			while (clipper.Step())
			{
				for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
					DrawEntity(context, traversal[static_cast<std::size_t>(index)], traversal, m_rangeAnchor);
			}
		}
		else
		{
			const auto appendSubtree = [&scene, &traversal](auto&& self, entt::entity entity) -> void
			{
				traversal.push_back(entity);
				for (const entt::entity child : scene.GetChildren(entity))
					self(self, child);
			};
			for (const entt::entity root : roots)
				appendSubtree(appendSubtree, root);
			for (const entt::entity root : roots)
				DrawEntity(context, root, traversal, m_rangeAnchor);
		}

		ImGui::InvisibleButton(
			"##HierarchyRootTarget",
			{ImGui::GetContentRegionAvail().x, std::max(24.0f, ImGui::GetContentRegionAvail().y)});
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENIGNE_ENTITY_ID"))
			{
				const std::uint64_t childID = *static_cast<const std::uint64_t*>(payload->Data);
				Reparent(context, scene.FindEntityByID(childID), entt::null);
			}
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetDragPayloadID))
			{
				const auto& asset = *static_cast<const AssetDragPayload*>(payload->Data);
				if (asset.Type == Graphics::AssetType::Prefab && context.InstantiatePrefab)
					context.InstantiatePrefab(asset.Handle, asset.Path.data(), 0, nullptr);
				else if (asset.Type == Graphics::AssetType::Model && context.InstantiateModel)
					context.InstantiateModel(asset.Handle, asset.Path.data(), 0, nullptr);
			}
			ImGui::EndDragDropTarget();
		}
		if (ImGui::BeginPopupContextItem("HierarchyCreateContext"))
		{
			DrawCreateMenu(context, entt::null);
			ImGui::EndPopup();
		}
		ImGui::End();
	}
}
