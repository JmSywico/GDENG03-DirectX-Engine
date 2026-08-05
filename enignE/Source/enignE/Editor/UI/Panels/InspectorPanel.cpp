#include "Editor/UI/Panels/InspectorPanel.h"

#include "Editor/Commands/CommandStack.h"
#include "Editor/Commands/EditorCommand.h"
#include "Editor/EditorContext.h"
#include "Editor/UI/UIScale.h"
#include "Editor/UI/Panels/AssetBrowserPanel.h"
#include "Graphics/AssetRegistry.h"
#include "Graphics/Material.h"
#include "Graphics/MaterialAsset.h"
#include "Graphics/Texture2D.h"
#include "Scene/Components/Components.h"
#include "Scene/Scene.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace
{
	using namespace enignE;

	Editor::TransformValue ReadTransform(const Scene::TransformComponent& transform)
	{
		return {
			transform.GetLocalPosition(),
			transform.GetLocalRotation(),
			transform.GetLocalScale(),
			transform.GetLocalRotationQuaternion(),
			true
		};
	}

	bool Equal(const DirectX::XMFLOAT3& left, const DirectX::XMFLOAT3& right)
	{
		return left.x == right.x && left.y == right.y && left.z == right.z;
	}

	bool DrawScaleLinkToggle(bool& linked)
	{
		ImGui::PushID("ScaleAxisLink");
		const float size = ImGui::GetFrameHeight();
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		const bool pressed = ImGui::InvisibleButton("##Link", {size, size});
		if (pressed) linked = !linked;
		const bool hovered = ImGui::IsItemHovered();
		const ImU32 background = ImGui::GetColorU32(
			linked ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
		const ImU32 stroke = ImGui::GetColorU32(
			linked ? ImGuiCol_Text : ImGuiCol_TextDisabled);
		ImDrawList* draw = ImGui::GetWindowDrawList();
		draw->AddRectFilled(origin, {origin.x + size, origin.y + size}, background, 2.0f);
		const float left = origin.x + size * 0.22f;
		const float right = origin.x + size * 0.78f;
		const float top = origin.y + size * 0.34f;
		const float bottom = origin.y + size * 0.66f;
		draw->AddRect({left, top}, {origin.x + size * 0.47f, bottom}, stroke, size * 0.14f, 0, 1.5f);
		draw->AddRect({origin.x + size * 0.53f, top}, {right, bottom}, stroke, size * 0.14f, 0, 1.5f);
		if (linked)
			draw->AddLine({origin.x + size * 0.43f, origin.y + size * 0.5f},
				{origin.x + size * 0.57f, origin.y + size * 0.5f}, stroke, 1.5f);
		if (hovered)
			ImGui::SetTooltip(linked
				? "Scale axes linked: adjustments preserve XYZ proportions"
				: "Scale axes independent");
		ImGui::PopID();
		return pressed;
	}

	float LinkedScaleFactor(const DirectX::XMFLOAT3& initial, const float edited[3])
	{
		const float values[3]{initial.x, initial.y, initial.z};
		for (int axis = 0; axis < 3; ++axis)
		{
			if (edited[axis] == values[axis]) continue;
			if (std::abs(values[axis]) > 1.0e-6f)
				return edited[axis] / values[axis];
			return 1.0f;
		}
		return 1.0f;
	}

	template<typename CommandFactory>
	void ExecuteBatch(Editor::EditorContext& context, CommandFactory&& factory, bool coalesced = false)
	{
		auto command = std::make_unique<Editor::CompositeCommand>();
		for (const entt::entity entity : context.GetSelectedEntities())
		{
			if (auto item = factory(entity))
				command->Add(std::move(item));
		}
		if (command->Empty() || !context.Commands)
			return;
		if (coalesced)
			context.Commands->ExecuteCoalesced(std::move(command));
		else
			context.Commands->Execute(std::move(command));
	}

	std::vector<entt::entity> TopLevelSelection(Editor::EditorContext& context)
	{
		std::vector<entt::entity> roots;
		for (const entt::entity candidate : context.GetSelectedEntities())
		{
			bool nested = false;
			for (entt::entity parent = context.ActiveScene->GetParent(candidate);
				parent != entt::null;
				parent = context.ActiveScene->GetParent(parent))
			{
				if (context.IsEntitySelected(parent))
				{
					nested = true;
					break;
				}
			}
			if (!nested)
				roots.push_back(candidate);
		}
		return roots;
	}

	std::optional<MaterialResource> RendererMaterial(
		const Scene::MeshRendererComponent& renderer)
	{
		return renderer.MaterialResourcePtr
			? std::optional<MaterialResource>(*renderer.MaterialResourcePtr)
			: std::nullopt;
	}

	bool LoadMaterialAsset(
		Editor::EditorContext& context,
		const std::filesystem::path& path,
		MaterialResource& material)
	{
		const std::filesystem::path resolvedPath =
			context.Assets ? context.Assets->ResolvePath(path) : path;
		return Graphics::MaterialAsset::Load(
			resolvedPath,
			material,
			[&context](std::uint64_t handle, const std::string& texturePath)
			{
				if (context.Assets)
				{
					std::shared_ptr<Texture2D> texture;
					if (handle != Graphics::InvalidAssetHandle)
						texture = context.Assets->LoadTexture(handle);
					if (!texture && !texturePath.empty())
						texture = context.Assets->LoadTexture(texturePath);
					return texture;
				}
				return context.LoadTexture && !texturePath.empty()
					? context.LoadTexture(texturePath)
					: nullptr;
			});
	}

	bool DrawMaterialAssetDropTarget(
		Editor::EditorContext& context,
		const char* label,
		MaterialResource& material,
		std::string* acceptedPath = nullptr)
	{
		bool accepted = false;
		ImGui::Button(label, {-1.0f, 0.0f});
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Drag a MAT asset from Asset Lens onto this field");
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
				Editor::AssetDragPayloadID);
				payload && payload->DataSize == sizeof(Editor::AssetDragPayload))
			{
				const auto& asset =
					*static_cast<const Editor::AssetDragPayload*>(payload->Data);
				if (asset.Type == Graphics::AssetType::Material
					&& LoadMaterialAsset(context, asset.Path.data(), material))
				{
					if (acceptedPath)
						*acceptedPath = asset.Path.data();
					accepted = true;
				}
			}
			ImGui::EndDragDropTarget();
		}
		return accepted;
	}

	void DrawMultiSelection(Editor::EditorContext& context, bool& linkScaleAxes)
	{
		auto& scene = *context.ActiveScene;
		const auto selection = context.GetSelectedEntities();
		ImGui::Text("%zu elements selected", selection.size());
		ImGui::TextDisabled("Values are applied to compatible selected elements.");

		std::vector<entt::entity> transforms;
		std::vector<entt::entity> renderers;
		for (const entt::entity entity : selection)
		{
			if (scene.HasComponent<Scene::TransformComponent>(entity))
				transforms.push_back(entity);
			if (scene.HasComponent<Scene::MeshRendererComponent>(entity))
				renderers.push_back(entity);
		}

		if (!transforms.empty())
		{
			const auto* primary = scene.GetComponent<Scene::TransformComponent>(transforms.back());
			ImGui::SeparatorText("Transform");
			ImGui::TextDisabled("%zu of %zu elements", transforms.size(), selection.size());
			const auto editVector = [&](const char* label, int field, float speed, float minimum, float maximum)
			{
				const Editor::TransformValue primaryValue = ReadTransform(*primary);
				const DirectX::XMFLOAT3 initial = field == 0
					? primaryValue.Position
					: field == 1 ? primaryValue.Rotation : primaryValue.Scale;
				bool mixed = false;
				for (const entt::entity entity : transforms)
				{
					const Editor::TransformValue value = ReadTransform(*scene.GetComponent<Scene::TransformComponent>(entity));
					const DirectX::XMFLOAT3 candidate = field == 0
						? value.Position
						: field == 1 ? value.Rotation : value.Scale;
					mixed = mixed || !Equal(initial, candidate);
				}
				float edited[3]{initial.x, initial.y, initial.z};
				if (field == 1)
				{
					edited[0] = DirectX::XMConvertToDegrees(edited[0]);
					edited[1] = DirectX::XMConvertToDegrees(edited[1]);
					edited[2] = DirectX::XMConvertToDegrees(edited[2]);
				}
				if (mixed)
					ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
				if (field == 2)
				{
					DrawScaleLinkToggle(linkScaleAxes);
					ImGui::SameLine();
				}
				const bool changed = ImGui::DragFloat3(label, edited, speed, minimum, maximum);
				if (mixed)
					ImGui::PopItemFlag();
				if (changed)
				{
					const float linkedFactor = field == 2 && linkScaleAxes
						? LinkedScaleFactor(initial, edited) : 1.0f;
					if (field == 2 && linkScaleAxes)
					{
						edited[0] = std::clamp(initial.x * linkedFactor, minimum, maximum);
						edited[1] = std::clamp(initial.y * linkedFactor, minimum, maximum);
						edited[2] = std::clamp(initial.z * linkedFactor, minimum, maximum);
					}
					DirectX::XMFLOAT3 target{edited[0], edited[1], edited[2]};
					if (field == 1)
					{
						target = {
							DirectX::XMConvertToRadians(target.x),
							DirectX::XMConvertToRadians(target.y),
							DirectX::XMConvertToRadians(target.z)};
					}
					DirectX::XMFLOAT3 baseline = initial;
					if (field == 1)
					{
						baseline = {
							DirectX::XMConvertToRadians(baseline.x),
							DirectX::XMConvertToRadians(baseline.y),
							DirectX::XMConvertToRadians(baseline.z)};
					}
					const DirectX::XMFLOAT3 delta{
						target.x - baseline.x,
						target.y - baseline.y,
						target.z - baseline.z};
					ExecuteBatch(context, [&scene, field, delta, linkedFactor, &linkScaleAxes](entt::entity entity)
						-> std::unique_ptr<Editor::EditorCommand>
					{
						const auto* transform = scene.GetComponent<Scene::TransformComponent>(entity);
						if (!transform)
							return {};
						const Editor::TransformValue before = ReadTransform(*transform);
						Editor::TransformValue after = before;
						if (field == 0)
						{
							after.Position.x += delta.x;
							after.Position.y += delta.y;
							after.Position.z += delta.z;
						}
						else if (field == 1)
						{
							after.Rotation.x += delta.x;
							after.Rotation.y += delta.y;
							after.Rotation.z += delta.z;
							after.HasRotationQuaternion = false;
						}
						else
						{
							if (linkScaleAxes)
							{
								after.Scale.x = std::clamp(after.Scale.x * linkedFactor, 0.001f, 1000.0f);
								after.Scale.y = std::clamp(after.Scale.y * linkedFactor, 0.001f, 1000.0f);
								after.Scale.z = std::clamp(after.Scale.z * linkedFactor, 0.001f, 1000.0f);
							}
							else
							{
								after.Scale.x = std::max(0.001f, after.Scale.x + delta.x);
								after.Scale.y = std::max(0.001f, after.Scale.y + delta.y);
								after.Scale.z = std::max(0.001f, after.Scale.z + delta.z);
							}
						}
						return std::make_unique<Editor::SetTransformCommand>(
							scene, scene.GetEntityID(entity), before, after);
					}, true);
				}
				if (ImGui::IsItemDeactivatedAfterEdit() && context.Commands)
					context.Commands->EndCoalescing();
			};
			editVector("Position", 0, 0.05f, 0.0f, 0.0f);
			editVector("Rotation", 1, 0.5f, 0.0f, 0.0f);
			editVector("Scale", 2, 0.02f, 0.001f, 1000.0f);
		}

		if (!renderers.empty())
		{
			const auto* primary = scene.GetComponent<Scene::MeshRendererComponent>(renderers.back());
			ImGui::SeparatorText("Mesh Renderer");
			ImGui::TextDisabled("%zu of %zu elements", renderers.size(), selection.size());
			bool visible = primary->bVisible;
			bool mixedVisible = false;
			for (const entt::entity entity : renderers)
				mixedVisible |= scene.GetComponent<Scene::MeshRendererComponent>(entity)->bVisible != visible;
			if (mixedVisible) ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
			const bool visibilityChanged = ImGui::Checkbox("Visible", &visible);
			if (mixedVisible) ImGui::PopItemFlag();
			if (visibilityChanged)
			{
				ExecuteBatch(context, [&scene, visible](entt::entity entity)
					-> std::unique_ptr<Editor::EditorCommand>
				{
					const auto* renderer = scene.GetComponent<Scene::MeshRendererComponent>(entity);
					if (!renderer) return {};
					return std::make_unique<Editor::SetRendererVisibilityCommand>(
						scene, scene.GetEntityID(entity), renderer->bVisible, visible);
				});
			}

			int material = static_cast<int>(primary->Material);
			bool mixedMaterial = false;
			for (const entt::entity entity : renderers)
				mixedMaterial |= scene.GetComponent<Scene::MeshRendererComponent>(entity)->Material != primary->Material;
			const char* materials[] = {"Lit Tint", "Rainbow", "Flat Green", "Flat Red", "Flat Blue"};
			if (mixedMaterial) ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
			const bool materialChanged = ImGui::Combo(
				"Shader mode", &material, materials, static_cast<int>(std::size(materials)));
			if (mixedMaterial) ImGui::PopItemFlag();
			if (materialChanged)
			{
				ExecuteBatch(context, [&scene, material](entt::entity entity)
					-> std::unique_ptr<Editor::EditorCommand>
				{
					const auto* renderer = scene.GetComponent<Scene::MeshRendererComponent>(entity);
					if (!renderer) return {};
					return std::make_unique<Editor::SetRendererMaterialCommand>(
						scene, scene.GetEntityID(entity), renderer->Material, static_cast<MaterialMode>(material));
				});
			}

			MaterialResource droppedMaterial;
			if (DrawMaterialAssetDropTarget(
				context, "Drop material on selected objects", droppedMaterial))
			{
				ExecuteBatch(context, [&scene, &droppedMaterial](entt::entity entity)
					-> std::unique_ptr<Editor::EditorCommand>
				{
					const auto* renderer = scene.GetComponent<Scene::MeshRendererComponent>(entity);
					if (!renderer) return {};
					return std::make_unique<Editor::SetRendererMaterialResourceCommand>(
						scene,
						scene.GetEntityID(entity),
						RendererMaterial(*renderer),
						droppedMaterial);
				});
			}
			const bool anyOverrides = std::any_of(
				renderers.begin(), renderers.end(), [&scene](entt::entity entity)
				{
					const auto* renderer =
						scene.GetComponent<Scene::MeshRendererComponent>(entity);
					return renderer && renderer->MaterialResourcePtr;
				});
			if (anyOverrides && ImGui::Button("Use imported model materials"))
			{
				ExecuteBatch(context, [&scene](entt::entity entity)
					-> std::unique_ptr<Editor::EditorCommand>
				{
					const auto* renderer = scene.GetComponent<Scene::MeshRendererComponent>(entity);
					if (!renderer || !renderer->MaterialResourcePtr) return {};
					return std::make_unique<Editor::SetRendererMaterialResourceCommand>(
						scene,
						scene.GetEntityID(entity),
						RendererMaterial(*renderer),
						std::nullopt);
				});
			}
		}

		ImGui::Separator();
		if (ImGui::Button("Delete Selected"))
		{
			auto command = std::make_unique<Editor::CompositeCommand>();
			for (const entt::entity entity : TopLevelSelection(context))
				command->Add(std::make_unique<Editor::DeleteEntityCommand>(scene, entity));
			if (!command->Empty() && context.Commands)
				context.Commands->Execute(std::move(command));
			context.ClearSelection();
		}
	}
}

namespace enignE::Editor
{
	void InspectorPanel::BeginTransformEdit(
		EditorContext& context,
		std::uint64_t entityID,
		const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT3& rotation,
		const DirectX::XMFLOAT4& rotationQuaternion,
		const DirectX::XMFLOAT3& scale)
	{
		if (m_transformEditActive)
			CommitTransformEdit(context);
		m_transformEditScene = context.ActiveScene;
		m_transformEditEntityID = entityID;
		m_transformEditPosition = position;
		m_transformEditRotation = rotation;
		m_transformEditRotationQuaternion = rotationQuaternion;
		m_transformEditScale = scale;
		m_transformEditActive = true;
	}

	void InspectorPanel::CommitTransformEdit(EditorContext& context)
	{
		if (!m_transformEditActive)
			return;

		Scene::Scene* scene = m_transformEditScene;
		const entt::entity entity =
			scene ? scene->FindEntityByID(m_transformEditEntityID) : entt::null;
		const auto* transform =
			scene ? scene->GetComponent<Scene::TransformComponent>(entity) : nullptr;
		if (transform && context.Commands)
		{
			const TransformValue before{
				m_transformEditPosition,
				m_transformEditRotation,
				m_transformEditScale,
				m_transformEditRotationQuaternion,
				true
			};
			const TransformValue after{
				transform->GetLocalPosition(),
				transform->GetLocalRotation(),
				transform->GetLocalScale(),
				transform->GetLocalRotationQuaternion(),
				true
			};
			const bool changed =
				before.Position.x != after.Position.x
				|| before.Position.y != after.Position.y
				|| before.Position.z != after.Position.z
				|| before.Rotation.x != after.Rotation.x
				|| before.Rotation.y != after.Rotation.y
				|| before.Rotation.z != after.Rotation.z
				|| before.Scale.x != after.Scale.x
				|| before.Scale.y != after.Scale.y
				|| before.Scale.z != after.Scale.z;
			if (changed)
			{
				context.Commands->RecordExecuted(std::make_unique<SetTransformCommand>(
					*scene,
					m_transformEditEntityID,
					before,
					after));
			}
		}

		m_transformEditScene = nullptr;
		m_transformEditEntityID = 0;
		m_transformEditActive = false;
	}

	void InspectorPanel::Draw(EditorContext& context)
	{
		if (!context.ActiveScene)
			return;

		Scene::Scene& scene = *context.ActiveScene;
		if (m_transformEditActive && m_transformEditScene != &scene)
		{
			m_transformEditScene = nullptr;
			m_transformEditEntityID = 0;
			m_transformEditActive = false;
		}
		const DirectX::XMUINT2 viewport = context.ViewportProviderCallback();
		ImGui::SetNextWindowPos(
			ImVec2(std::max(UI::Scale(12.0f), static_cast<float>(viewport.x) - UI::Scale(332.0f)), UI::Scale(92.0f)),
			ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(
			ImVec2(UI::Scale(320.0f), std::max(UI::Scale(280.0f), static_cast<float>(viewport.y) - UI::Scale(104.0f))),
			ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Inspector"))
		{
			ImGui::End();
			return;
		}

		if (!context.IsSelectedEntityValid())
		{
			CommitTransformEdit(context);
			ImGui::TextDisabled("Select an element in the scene or element list.");
			ImGui::End();
			return;
		}

		const entt::entity entity = context.SelectedEntity;
		const std::uint64_t id = scene.GetEntityID(entity);
		const bool editable = !context.GetSimulationState
			|| context.GetSimulationState() == SimulationState::Stopped;
		if (!editable)
			ImGui::TextColored(ImVec4(0.95f, 0.68f, 0.24f, 1.0f),
				"Runtime view - stop simulation to author components");
		if (context.GetSelectedEntities().size() > 1)
		{
			CommitTransformEdit(context);
			if (!editable) ImGui::BeginDisabled();
			DrawMultiSelection(context, m_linkScaleAxes);
			if (!editable) ImGui::EndDisabled();
			ImGui::End();
			return;
		}
		if (m_transformEditActive && m_transformEditEntityID != id)
			CommitTransformEdit(context);
		ImGui::TextDisabled("ID: %llu", static_cast<unsigned long long>(id));
		if (!editable) ImGui::BeginDisabled();

		if (const auto* tag = scene.GetComponent<Scene::TagComponent>(entity))
		{
			std::array<char, Scene::TagComponent::MaxTagLength> name{};
			strncpy_s(name.data(), name.size(), tag->Tag, name.size() - 1);
			if (ImGui::InputText("Name", name.data(), name.size(), ImGuiInputTextFlags_EnterReturnsTrue)
				&& context.Commands)
			{
				context.Commands->Execute(std::make_unique<RenameEntityCommand>(
					scene, id, tag->Tag, name.data()));
			}
		}

		if (const auto* transform = scene.GetComponent<Scene::TransformComponent>(entity))
		{
			ImGui::SeparatorText("Transform");
			const TransformValue before{
				transform->GetLocalPosition(),
				transform->GetLocalRotation(),
				transform->GetLocalScale(),
				transform->GetLocalRotationQuaternion(),
				true
			};
			TransformValue after = before;
			float position[3]{after.Position.x, after.Position.y, after.Position.z};
			float rotation[3]{
				DirectX::XMConvertToDegrees(after.Rotation.x),
				DirectX::XMConvertToDegrees(after.Rotation.y),
				DirectX::XMConvertToDegrees(after.Rotation.z)
			};
			float scale[3]{after.Scale.x, after.Scale.y, after.Scale.z};
			const bool positionChanged = ImGui::DragFloat3("Position", position, 0.05f);
			if (ImGui::IsItemActivated())
			{
				BeginTransformEdit(
					context,
					id,
					before.Position,
					before.Rotation,
					transform->GetLocalRotationQuaternion(),
					before.Scale);
			}
			if (positionChanged)
			{
				after.Position = {position[0], position[1], position[2]};
				scene.SetLocalPosition(entity, after.Position);
			}
			if (ImGui::IsItemDeactivatedAfterEdit())
				CommitTransformEdit(context);

			const bool rotationChanged = ImGui::DragFloat3("Rotation", rotation, 0.5f);
			if (ImGui::IsItemActivated())
			{
				BeginTransformEdit(
					context,
					id,
					before.Position,
					before.Rotation,
					transform->GetLocalRotationQuaternion(),
					before.Scale);
			}
			if (rotationChanged)
			{
				const DirectX::XMFLOAT3 rotationHint{
					DirectX::XMConvertToRadians(rotation[0]),
					DirectX::XMConvertToRadians(rotation[1]),
					DirectX::XMConvertToRadians(rotation[2])
				};
				const DirectX::XMFLOAT3 delta{
					rotationHint.x - before.Rotation.x,
					rotationHint.y - before.Rotation.y,
					rotationHint.z - before.Rotation.z
				};
				DirectX::XMVECTOR quaternion =
					DirectX::XMLoadFloat4(&transform->GetLocalRotationQuaternion());
				if (delta.x != 0.0f)
				{
					quaternion = DirectX::XMQuaternionMultiply(
						DirectX::XMQuaternionRotationAxis(
							DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
							delta.x),
						quaternion);
				}
				if (delta.y != 0.0f)
				{
					quaternion = DirectX::XMQuaternionMultiply(
						DirectX::XMQuaternionRotationAxis(
							DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
							delta.y),
						quaternion);
				}
				if (delta.z != 0.0f)
				{
					quaternion = DirectX::XMQuaternionMultiply(
						DirectX::XMQuaternionRotationAxis(
							DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
							delta.z),
						quaternion);
				}
				DirectX::XMFLOAT4 rotationQuaternion{};
				DirectX::XMStoreFloat4(
					&rotationQuaternion,
					DirectX::XMQuaternionNormalize(quaternion));
				scene.SetLocalRotationQuaternion(entity, rotationQuaternion, rotationHint);
			}
			if (ImGui::IsItemDeactivatedAfterEdit())
				CommitTransformEdit(context);

			DrawScaleLinkToggle(m_linkScaleAxes);
			ImGui::SameLine();
			const bool scaleChanged =
				ImGui::DragFloat3("Scale", scale, 0.02f, 0.001f, 1000.0f);
			if (ImGui::IsItemActivated())
			{
				BeginTransformEdit(
					context,
					id,
					before.Position,
					before.Rotation,
					transform->GetLocalRotationQuaternion(),
					before.Scale);
			}
			if (scaleChanged)
			{
				if (m_linkScaleAxes)
				{
					const float factor = LinkedScaleFactor(before.Scale, scale);
					scale[0] = std::clamp(before.Scale.x * factor, 0.001f, 1000.0f);
					scale[1] = std::clamp(before.Scale.y * factor, 0.001f, 1000.0f);
					scale[2] = std::clamp(before.Scale.z * factor, 0.001f, 1000.0f);
				}
				after.Scale = {scale[0], scale[1], scale[2]};
				scene.SetLocalScale(entity, after.Scale);
			}
			if (ImGui::IsItemDeactivatedAfterEdit())
				CommitTransformEdit(context);
		}

		if (const auto* renderer = scene.GetComponent<Scene::MeshRendererComponent>(entity))
		{
			ImGui::SeparatorText("Mesh Renderer");
			ImGui::Button("Drop model asset", ImVec2(-1.0f, 0.0f));
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetDragPayloadID))
				{
					const auto& asset = *static_cast<const AssetDragPayload*>(payload->Data);
					if (asset.Type == Graphics::AssetType::Model && context.LoadModel && context.Commands)
					{
						if (auto model = context.LoadModel(asset.Path.data()))
						{
							std::optional<Scene::RenderSourceComponent> beforeSource;
							if (const auto* source = scene.GetComponent<Scene::RenderSourceComponent>(entity))
								beforeSource = *source;
							Scene::RenderSourceComponent afterSource;
							afterSource.Type = Scene::RenderSourceType::ImportedModel;
							afterSource.AssetHandle = asset.Handle;
							afterSource.AssetPath = asset.Path.data();
							context.Commands->Execute(std::make_unique<SetRendererModelCommand>(
								scene, id, renderer->ModelPtr, beforeSource,
								std::move(model), afterSource));
						}
					}
				}
				ImGui::EndDragDropTarget();
			}
			bool visible = renderer->bVisible;
			if (ImGui::Checkbox("Visible", &visible) && context.Commands)
			{
				context.Commands->Execute(std::make_unique<SetRendererVisibilityCommand>(
					scene, id, renderer->bVisible, visible));
			}
			bool castShadows = renderer->bCastShadows;
			if (ImGui::Checkbox("Casts shadows", &castShadows) && context.Commands)
			{
				context.Commands->Execute(std::make_unique<SetRendererShadowFlagsCommand>(
					scene,
					id,
					renderer->bCastShadows,
					renderer->bReceiveShadows,
					castShadows,
					renderer->bReceiveShadows));
			}
			bool receiveShadows = renderer->bReceiveShadows;
			if (ImGui::Checkbox("Receives shadows", &receiveShadows) && context.Commands)
			{
				context.Commands->Execute(std::make_unique<SetRendererShadowFlagsCommand>(
					scene,
					id,
					renderer->bCastShadows,
					renderer->bReceiveShadows,
					renderer->bCastShadows,
					receiveShadows));
			}
			DirectX::XMFLOAT4 albedo = renderer->Albedo;
			if (ImGui::ColorEdit4("Albedo", &albedo.x) && context.Commands)
			{
				context.Commands->ExecuteCoalesced(std::make_unique<SetRendererAlbedoCommand>(
					scene, id, renderer->Albedo, albedo));
			}
			if (ImGui::IsItemDeactivatedAfterEdit() && context.Commands)
				context.Commands->EndCoalescing();
			int material = static_cast<int>(renderer->Material);
			const char* materials[] = {"Lit Tint", "Rainbow", "Flat Green", "Flat Red", "Flat Blue"};
			if (ImGui::Combo("Shader mode", &material, materials, static_cast<int>(std::size(materials)))
				&& context.Commands)
			{
				context.Commands->Execute(std::make_unique<SetRendererMaterialCommand>(
					scene, id, renderer->Material, static_cast<MaterialMode>(material)));
			}

			static std::unordered_map<std::uint64_t, std::array<char, 512>> materialPaths;
			auto [pathIterator, inserted] = materialPaths.try_emplace(id);
			if (inserted)
				strncpy_s(pathIterator->second.data(), pathIterator->second.size(),
					"assets/materials/material.ematerial", _TRUNCATE);
			MaterialResource droppedMaterial;
			std::string droppedPath;
			ImGui::TextDisabled(renderer->MaterialResourcePtr
				? "Material override active"
				: "Using imported model material");
			if (DrawMaterialAssetDropTarget(
				context, "Drop material on this object", droppedMaterial, &droppedPath)
				&& context.Commands)
			{
				strncpy_s(pathIterator->second.data(), pathIterator->second.size(),
					droppedPath.c_str(), _TRUNCATE);
				context.Commands->Execute(
					std::make_unique<SetRendererMaterialResourceCommand>(
						scene, id, RendererMaterial(*renderer), std::move(droppedMaterial)));
			}

			if (!renderer->MaterialResourcePtr)
			{
				if (ImGui::Button("Create PBR surface") && context.Commands)
				{
					MaterialResource created;
					created.Albedo = renderer->Albedo;
					context.Commands->Execute(
						std::make_unique<SetRendererMaterialResourceCommand>(
							scene, id, std::nullopt, created));
				}
			}
			else if (ImGui::TreeNodeEx("Surface response", ImGuiTreeNodeFlags_DefaultOpen))
			{
				const MaterialResource before = *renderer->MaterialResourcePtr;
				MaterialResource after = before;
				ImGui::SetNextItemWidth(-1.0f);
				ImGui::InputText("Material asset", pathIterator->second.data(), pathIterator->second.size());
				bool replaced = false;
				const auto loadMaterialAsset = [&](const char* materialPath)
				{
					MaterialResource loaded;
					if (!LoadMaterialAsset(context, materialPath, loaded)) return;
					if (context.Commands)
						context.Commands->Execute(
							std::make_unique<SetRendererMaterialResourceCommand>(
								scene, id, before, std::move(loaded)));
					replaced = true;
				};
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetDragPayloadID))
					{
						const auto& asset = *static_cast<const AssetDragPayload*>(payload->Data);
						if (asset.Type == Graphics::AssetType::Material)
						{
							strncpy_s(pathIterator->second.data(), pathIterator->second.size(),
								asset.Path.data(), _TRUNCATE);
							loadMaterialAsset(asset.Path.data());
						}
					}
					ImGui::EndDragDropTarget();
				}
				if (ImGui::Button("Save asset"))
				{
					const std::filesystem::path path = context.Assets
						? context.Assets->ResolvePath(pathIterator->second.data())
						: std::filesystem::path(pathIterator->second.data());
					Graphics::MaterialAsset::Save(before, path);
					if (context.Assets)
						context.Assets->DiscoverAssets(context.AssetDirectory);
				}
				ImGui::SameLine();
				if (ImGui::Button("Load asset"))
					loadMaterialAsset(pathIterator->second.data());
				ImGui::SameLine();
				if (ImGui::Button("Use model materials") && context.Commands)
				{
					context.Commands->Execute(
						std::make_unique<SetRendererMaterialResourceCommand>(
							scene, id, before, std::nullopt));
					replaced = true;
				}
				ImGui::Separator();
				bool changed = false;
				bool editFinished = false;
				if (!replaced)
				{
					changed |= ImGui::ColorEdit4("Base color", &after.Albedo.x);
					editFinished |= ImGui::IsItemDeactivatedAfterEdit();
					changed |= ImGui::ColorEdit3("Emissive", &after.Emissive.x);
					editFinished |= ImGui::IsItemDeactivatedAfterEdit();
					changed |= ImGui::SliderFloat("Metallic", &after.Metallic, 0.0f, 1.0f);
					editFinished |= ImGui::IsItemDeactivatedAfterEdit();
					changed |= ImGui::SliderFloat("Roughness", &after.Roughness, 0.04f, 1.0f);
					editFinished |= ImGui::IsItemDeactivatedAfterEdit();
				}

				const auto textureSlot = [&](const char* label,
					std::string& path, std::uint64_t& handle, std::shared_ptr<Texture2D>& texture)
				{
					ImGui::PushID(label);
					char pathBuffer[512] = {};
					strncpy_s(pathBuffer, path.c_str(), _TRUNCATE);
					ImGui::SetNextItemWidth(-72.0f);
					if (ImGui::InputText(label, pathBuffer, std::size(pathBuffer)))
					{
						path = pathBuffer;
						handle = 0;
						changed = true;
					}
					editFinished |= ImGui::IsItemDeactivatedAfterEdit();
					if (ImGui::BeginDragDropTarget())
					{
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetDragPayloadID))
						{
							const auto& asset = *static_cast<const AssetDragPayload*>(payload->Data);
							if (asset.Type == Graphics::AssetType::Texture && context.LoadTexture)
							{
								if (auto loaded = context.LoadTexture(asset.Path.data()))
								{
									path = asset.Path.data();
									handle = asset.Handle;
									texture = std::move(loaded);
									changed = true;
									editFinished = true;
								}
							}
						}
						ImGui::EndDragDropTarget();
					}
					ImGui::SameLine();
					if (ImGui::Button("Load") && context.LoadTexture && !path.empty())
					{
						if (auto loaded = context.LoadTexture(path))
						{
							texture = std::move(loaded);
							changed = true;
							editFinished = true;
						}
					}
					ImGui::PopID();
				};
				if (!replaced)
				{
					textureSlot("Albedo map", after.AlbedoTexturePath,
						after.AlbedoTextureHandle, after.AlbedoTexture);
					textureSlot("Normal map", after.NormalTexturePath,
						after.NormalTextureHandle, after.NormalTexture);
					textureSlot("Metal/Rough map", after.MetallicRoughnessTexturePath,
						after.MetallicRoughnessTextureHandle, after.MetallicRoughnessTexture);
				}
				if (changed && context.Commands)
					context.Commands->ExecuteCoalesced(
						std::make_unique<SetRendererMaterialResourceCommand>(
							scene, id, before, after));
				if (editFinished && context.Commands)
					context.Commands->EndCoalescing();
				ImGui::TreePop();
			}
		}

		if (const auto* prefab = scene.GetComponent<Scene::PrefabInstanceComponent>(entity))
		{
			ImGui::SeparatorText("Prefab Instance");
			ImGui::TextWrapped("%s", prefab->PrefabAssetPath.c_str());
			ImGui::TextDisabled("Local ID: %llu  Overrides: 0x%02x",
				static_cast<unsigned long long>(prefab->PrefabLocalID), prefab->OverrideMask);
			if (context.Commands && ImGui::Button("Unpack Instance"))
				context.Commands->Execute(std::make_unique<UnpackPrefabCommand>(scene, entity));
		}

		if (const auto* camera = scene.GetComponent<Scene::CameraComponent>(entity))
		{
			ImGui::SeparatorText("Camera");
			const auto editCameraFloat = [&](const char* label, float Scene::CameraComponent::* member,
				float speed, float minimum, float maximum)
			{
				Scene::CameraComponent after = *camera;
				if (ImGui::DragFloat(label, &(after.*member), speed, minimum, maximum)
					&& context.Commands)
				{
					after.NearPlane = std::max(0.001f, after.NearPlane);
					after.FarPlane = std::max(after.NearPlane + 0.001f, after.FarPlane);
					context.Commands->ExecuteCoalesced(std::make_unique<SetCameraCommand>(
						scene, id, *camera, after));
				}
				if (ImGui::IsItemDeactivatedAfterEdit() && context.Commands)
					context.Commands->EndCoalescing();
			};
			editCameraFloat("FOV", &Scene::CameraComponent::FOVDegrees, 0.25f, 1.0f, 179.0f);
			editCameraFloat(
				"Near Clip", &Scene::CameraComponent::NearPlane, 0.01f, 0.001f,
				std::max(0.001f, camera->FarPlane - 0.001f));
			editCameraFloat(
				"Far Clip", &Scene::CameraComponent::FarPlane, 1.0f,
				camera->NearPlane + 0.001f, 100000.0f);
			editCameraFloat(
				"Aspect Ratio", &Scene::CameraComponent::AspectRatio, 0.01f,
				0.1f, 10.0f);
			bool primary = camera->bPrimary;
			if (ImGui::Checkbox("Primary", &primary) && context.Commands)
			{
				Scene::CameraComponent after = *camera;
				after.bPrimary = primary;
				context.Commands->Execute(std::make_unique<SetCameraCommand>(
					scene, id, *camera, after));
			}
			if (scene.GetSettings().ActiveCameraEntityID == id)
				ImGui::TextDisabled("Active game camera");
			else if (ImGui::Button("Set Active Camera") && context.Commands)
				context.Commands->Execute(std::make_unique<SetActiveCameraCommand>(
					scene, scene.GetSettings().ActiveCameraEntityID, id));
		}

		if (const auto* light = scene.GetComponent<Scene::LightComponent>(entity))
		{
			ImGui::SeparatorText("Light");
			int lightType = static_cast<int>(light->LightType);
			const char* lightTypes[] = {"Directional", "Point", "Spot"};
			if (ImGui::Combo("Type", &lightType, lightTypes, static_cast<int>(std::size(lightTypes)))
				&& context.Commands)
			{
				Scene::LightComponent after = *light;
				after.LightType = static_cast<Scene::LightComponent::Type>(lightType);
				context.Commands->Execute(std::make_unique<SetLightCommand>(
					scene, id, *light, after));
			}
			Scene::LightComponent edited = *light;
			if (ImGui::ColorEdit3("Color", &edited.Color.x) && context.Commands)
				context.Commands->ExecuteCoalesced(std::make_unique<SetLightCommand>(
					scene, id, *light, edited));
			if (ImGui::IsItemDeactivatedAfterEdit() && context.Commands)
				context.Commands->EndCoalescing();
			edited = *scene.GetComponent<Scene::LightComponent>(entity);
			if (ImGui::DragFloat("Intensity", &edited.Intensity, 0.05f, 0.0f, 100.0f)
				&& context.Commands)
				context.Commands->ExecuteCoalesced(std::make_unique<SetLightCommand>(
					scene, id, *light, edited));
			if (ImGui::IsItemDeactivatedAfterEdit() && context.Commands)
				context.Commands->EndCoalescing();
			edited = *scene.GetComponent<Scene::LightComponent>(entity);
			if (ImGui::DragFloat("Range", &edited.Range, 0.25f, 0.0f, 10000.0f)
				&& context.Commands)
				context.Commands->ExecuteCoalesced(std::make_unique<SetLightCommand>(
					scene, id, *light, edited));
			if (ImGui::IsItemDeactivatedAfterEdit() && context.Commands)
				context.Commands->EndCoalescing();
			if (light->LightType == Scene::LightComponent::Type::Spot)
			{
				edited = *scene.GetComponent<Scene::LightComponent>(entity);
				if (ImGui::DragFloat("Spot Angle", &edited.SpotAngle, 0.25f, 1.0f, 179.0f)
					&& context.Commands)
					context.Commands->ExecuteCoalesced(std::make_unique<SetLightCommand>(
						scene, id, *light, edited));
				if (ImGui::IsItemDeactivatedAfterEdit() && context.Commands)
					context.Commands->EndCoalescing();
			}
			if (light->LightType == Scene::LightComponent::Type::Point)
			{
				ImGui::TextDisabled("Point-light shadows are not available");
			}
			else
			{
				ImGui::SeparatorText("Shadows");
				edited = *scene.GetComponent<Scene::LightComponent>(entity);
				if (ImGui::Checkbox("Cast shadows", &edited.bCastShadows) && context.Commands)
					context.Commands->Execute(std::make_unique<SetLightCommand>(scene, id, *light, edited));
				edited = *scene.GetComponent<Scene::LightComponent>(entity);
				if (ImGui::DragFloat("Strength", &edited.ShadowStrength, 0.01f, 0.0f, 1.0f)
					&& context.Commands)
					context.Commands->ExecuteCoalesced(std::make_unique<SetLightCommand>(scene, id, *light, edited));
				if (ImGui::IsItemDeactivatedAfterEdit() && context.Commands) context.Commands->EndCoalescing();
				edited = *scene.GetComponent<Scene::LightComponent>(entity);
				if (ImGui::DragFloat("Depth bias", &edited.ShadowBias, 0.00005f, 0.0f, 0.05f, "%.5f")
					&& context.Commands)
					context.Commands->ExecuteCoalesced(std::make_unique<SetLightCommand>(scene, id, *light, edited));
				if (ImGui::IsItemDeactivatedAfterEdit() && context.Commands) context.Commands->EndCoalescing();
				edited = *scene.GetComponent<Scene::LightComponent>(entity);
				if (ImGui::DragFloat("Normal bias", &edited.ShadowNormalBias, 0.002f, 0.0f, 1.0f, "%.3f")
					&& context.Commands)
					context.Commands->ExecuteCoalesced(std::make_unique<SetLightCommand>(scene, id, *light, edited));
				if (ImGui::IsItemDeactivatedAfterEdit() && context.Commands) context.Commands->EndCoalescing();
				if (light->LightType == Scene::LightComponent::Type::Directional)
				{
					edited = *scene.GetComponent<Scene::LightComponent>(entity);
					if (ImGui::DragFloat("Distance", &edited.ShadowDistance, 1.0f, 1.0f, 10000.0f)
						&& context.Commands)
						context.Commands->ExecuteCoalesced(std::make_unique<SetLightCommand>(scene, id, *light, edited));
					if (ImGui::IsItemDeactivatedAfterEdit() && context.Commands) context.Commands->EndCoalescing();
				}
			}
			if (scene.GetSettings().ActiveLightEntityID == id)
				ImGui::TextDisabled("Active scene light");
			else if (ImGui::Button("Set Active Light") && context.Commands)
			{
				context.Commands->Execute(std::make_unique<SetActiveLightCommand>(
					scene, scene.GetSettings().ActiveLightEntityID, id));
			}
		}

		if (const auto* rotator = scene.GetComponent<Scene::RotatorComponent>(entity))
		{
			ImGui::SeparatorText("Rotator");
			Scene::RotatorComponent edited = *rotator;
			if (ImGui::Checkbox("Enabled##Rotator", &edited.Enabled) && context.Commands)
				context.Commands->Execute(std::make_unique<SetRotatorCommand>(
					scene, id, *rotator, edited));
			edited = *scene.GetComponent<Scene::RotatorComponent>(entity);
			float degrees[3]{
				DirectX::XMConvertToDegrees(edited.AngularVelocity.x),
				DirectX::XMConvertToDegrees(edited.AngularVelocity.y),
				DirectX::XMConvertToDegrees(edited.AngularVelocity.z)};
			if (ImGui::DragFloat3("Degrees / second", degrees, 1.0f, -3600.0f, 3600.0f))
			{
				edited.AngularVelocity = {
					DirectX::XMConvertToRadians(degrees[0]),
					DirectX::XMConvertToRadians(degrees[1]),
					DirectX::XMConvertToRadians(degrees[2])};
				if (context.Commands)
					context.Commands->ExecuteCoalesced(std::make_unique<SetRotatorCommand>(
						scene, id, *rotator, edited));
			}
			if (ImGui::IsItemDeactivatedAfterEdit() && context.Commands)
				context.Commands->EndCoalescing();
			if (ImGui::Button("Remove Rotator") && context.Commands)
				context.Commands->Execute(std::make_unique<RemoveRotatorCommand>(scene, id, *rotator));
		}

		if (const auto* fly = scene.GetComponent<Scene::FlyControllerComponent>(entity))
		{
			ImGui::SeparatorText("Fly Controller");
			Scene::FlyControllerComponent edited = *fly;
			if (ImGui::Checkbox("Enabled##Fly", &edited.Enabled) && context.Commands)
				context.Commands->Execute(std::make_unique<SetFlyControllerCommand>(scene, id, *fly, edited));
			const auto editFloat = [&](const char* label, float Scene::FlyControllerComponent::* member,
				float speed, float minimum, float maximum, const char* format = "%.3f")
			{
				Scene::FlyControllerComponent value = *scene.GetComponent<Scene::FlyControllerComponent>(entity);
				if (ImGui::DragFloat(label, &(value.*member), speed, minimum, maximum, format)
					&& context.Commands)
					context.Commands->ExecuteCoalesced(std::make_unique<SetFlyControllerCommand>(
						scene, id, *scene.GetComponent<Scene::FlyControllerComponent>(entity), value));
				if (ImGui::IsItemDeactivatedAfterEdit() && context.Commands) context.Commands->EndCoalescing();
			};
			editFloat("Move speed", &Scene::FlyControllerComponent::MoveSpeed, 0.1f, 0.0f, 1000.0f);
			editFloat("Look sensitivity", &Scene::FlyControllerComponent::LookSensitivity, 0.0001f, 0.0f, 1.0f, "%.4f");
			editFloat("Boost multiplier", &Scene::FlyControllerComponent::BoostMultiplier, 0.1f, 1.0f, 100.0f);
			editFloat("Pitch limit", &Scene::FlyControllerComponent::PitchLimitDegrees, 0.25f, 1.0f, 89.9f);
			ImGui::TextDisabled("WASD move  Q/E vertical  Shift boost  Mouse look");
			if (ImGui::Button("Remove Fly Controller") && context.Commands)
				context.Commands->Execute(std::make_unique<RemoveFlyControllerCommand>(scene, id, *fly));
		}

		if (const auto* body = scene.GetComponent<Scene::RigidBodyComponent>(entity))
		{
			ImGui::SeparatorText("Rigid Body");
			Scene::RigidBodyComponent edited = *body;
			const char* motionNames[] = {"Static", "Dynamic", "Kinematic"};
			int motion = static_cast<int>(edited.Motion);
			if (ImGui::Combo("Motion", &motion, motionNames, 3) && context.Commands)
			{
				edited.Motion = static_cast<Scene::RigidBodyMotion>(motion);
				context.Commands->Execute(std::make_unique<SetRigidBodyCommand>(scene, id, *body, edited));
			}
			edited = *scene.GetComponent<Scene::RigidBodyComponent>(entity);
			if (ImGui::Checkbox("Enabled##RigidBody", &edited.Enabled) && context.Commands)
				context.Commands->Execute(std::make_unique<SetRigidBodyCommand>(scene, id, *body, edited));
			const auto editBodyFloat = [&](const char* label,
				float Scene::RigidBodyComponent::* member, float speed, float minimum, float maximum)
			{
				Scene::RigidBodyComponent value = *scene.GetComponent<Scene::RigidBodyComponent>(entity);
				if (ImGui::DragFloat(label, &(value.*member), speed, minimum, maximum, "%.3f")
					&& context.Commands)
					context.Commands->ExecuteCoalesced(std::make_unique<SetRigidBodyCommand>(
						scene, id, *scene.GetComponent<Scene::RigidBodyComponent>(entity), value));
				if (ImGui::IsItemDeactivatedAfterEdit() && context.Commands) context.Commands->EndCoalescing();
			};
			editBodyFloat("Friction", &Scene::RigidBodyComponent::Friction, 0.01f, 0.0f, 10.0f);
			editBodyFloat("Restitution", &Scene::RigidBodyComponent::Restitution, 0.01f, 0.0f, 1.0f);
			editBodyFloat("Linear damping", &Scene::RigidBodyComponent::LinearDamping, 0.01f, 0.0f, 10.0f);
			editBodyFloat("Angular damping", &Scene::RigidBodyComponent::AngularDamping, 0.01f, 0.0f, 10.0f);
			editBodyFloat("Gravity factor", &Scene::RigidBodyComponent::GravityFactor, 0.01f, -10.0f, 10.0f);
			if (body->Motion == Scene::RigidBodyMotion::Dynamic && scene.GetParent(entity) != entt::null)
				ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "Dynamic bodies must be hierarchy roots");
			if (ImGui::Button("Remove Rigid Body") && context.Commands)
				context.Commands->Execute(std::make_unique<RemoveRigidBodyCommand>(scene, id, *body));
		}

		if (const auto* collider = scene.GetComponent<Scene::ColliderComponent>(entity))
		{
			ImGui::SeparatorText("Collider");
			Scene::ColliderComponent edited = *collider;
			const char* shapeNames[] = {"Box", "Sphere"};
			int shape = static_cast<int>(edited.Shape);
			if (ImGui::Combo("Shape", &shape, shapeNames, 2) && context.Commands)
			{
				edited.Shape = static_cast<Scene::ColliderShape>(shape);
				context.Commands->Execute(std::make_unique<SetColliderCommand>(scene, id, *collider, edited));
			}
			edited = *scene.GetComponent<Scene::ColliderComponent>(entity);
			if (edited.Shape == Scene::ColliderShape::Box)
			{
				float extents[3]{edited.HalfExtents.x, edited.HalfExtents.y, edited.HalfExtents.z};
				if (ImGui::DragFloat3("Half extents", extents, 0.01f, 0.001f, 10000.0f)
					&& context.Commands)
				{
					edited.HalfExtents = {extents[0], extents[1], extents[2]};
					context.Commands->ExecuteCoalesced(std::make_unique<SetColliderCommand>(
						scene, id, *scene.GetComponent<Scene::ColliderComponent>(entity), edited));
				}
			}
			else if (ImGui::DragFloat("Radius", &edited.Radius, 0.01f, 0.001f, 10000.0f)
				&& context.Commands)
				context.Commands->ExecuteCoalesced(std::make_unique<SetColliderCommand>(
					scene, id, *scene.GetComponent<Scene::ColliderComponent>(entity), edited));
			if (ImGui::IsItemDeactivatedAfterEdit() && context.Commands) context.Commands->EndCoalescing();
			if (ImGui::Button("Remove Collider") && context.Commands)
				context.Commands->Execute(std::make_unique<RemoveColliderCommand>(scene, id, *collider));
		}

		ImGui::Separator();
		if (ImGui::Button("Add Component", ImVec2(-1.0f, 0.0f)))
			ImGui::OpenPopup("##AddComponent");
		if (ImGui::BeginPopup("##AddComponent"))
		{
			for (const Scene::ComponentDescriptor& descriptor : Scene::ComponentCatalog::Descriptors())
			{
				const bool present = Scene::ComponentCatalog::Has(scene, entity, descriptor.Kind);
				if (ImGui::MenuItem(descriptor.Name.data(), descriptor.Category.data(), false, !present)
					&& context.Commands)
					context.Commands->Execute(std::make_unique<AddComponentCommand>(
						scene, id, descriptor.Kind));
			}
			ImGui::EndPopup();
		}

		ImGui::Separator();
		if (ImGui::Button("Delete Element"))
		{
			if (context.Commands)
			{
				context.Commands->Execute(std::make_unique<DeleteEntityCommand>(scene, entity));
			}
			context.ClearSelection();
		}
		if (!editable) ImGui::EndDisabled();
		ImGui::End();
	}
}
