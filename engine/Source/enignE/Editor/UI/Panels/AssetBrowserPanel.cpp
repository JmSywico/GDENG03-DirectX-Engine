#include "Editor/UI/Panels/AssetBrowserPanel.h"

#include "Editor/EditorContext.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace enignE::Editor
{
	namespace
	{
		const char* TypeName(Graphics::AssetType type)
		{
			switch (type)
			{
			case Graphics::AssetType::Model: return "MODEL";
			case Graphics::AssetType::Texture: return "TEX";
			case Graphics::AssetType::Material: return "MAT";
			case Graphics::AssetType::Scene: return "SCENE";
			case Graphics::AssetType::Prefab: return "PREFAB";
			default: return "?";
			}
		}
	}

	void AssetBrowserPanel::Draw(EditorContext& context)
	{
		if (!ImGui::Begin("Asset Lens")) { ImGui::End(); return; }
		if (!context.Assets)
		{
			ImGui::TextDisabled("Asset registry unavailable");
			ImGui::End();
			return;
		}
		ImGui::SetNextItemWidth(-80.0f);
		ImGui::InputTextWithHint("##AssetFilter", "filter assets", m_filter.data(), m_filter.size());
		ImGui::SameLine();
		if (ImGui::Button("Scan")) context.Assets->DiscoverAssets(context.AssetDirectory);

		std::string filter = m_filter.data();
		std::transform(filter.begin(), filter.end(), filter.begin(),
			[](unsigned char value) { return static_cast<char>(std::tolower(value)); });
		if (ImGui::BeginTable("##AssetTable", 3,
			ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV
			| ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 58.0f);
			ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 72.0f);
			ImGui::TableHeadersRow();
			for (const Graphics::AssetReference& asset : context.Assets->GetReferences())
			{
				std::string searchable = asset.Path;
				std::transform(searchable.begin(), searchable.end(), searchable.begin(),
					[](unsigned char value) { return static_cast<char>(std::tolower(value)); });
				if (!filter.empty() && searchable.find(filter) == std::string::npos) continue;
				ImGui::PushID(static_cast<int>(asset.Handle));
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", TypeName(asset.Type));
				ImGui::TableSetColumnIndex(1);
				const std::string name = std::filesystem::path(asset.Path).filename().string();
				const bool selected = ImGui::Selectable(
					name.c_str(), false,
					ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick);
				if (selected && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)
					&& asset.Type == Graphics::AssetType::Prefab && context.InstantiatePrefab)
					context.InstantiatePrefab(asset.Handle, asset.Path, 0, nullptr);
				else if (selected && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)
					&& asset.Type == Graphics::AssetType::Model && context.InstantiateModel)
					context.InstantiateModel(asset.Handle, asset.Path, 0, nullptr);
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", asset.Path.c_str());
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
				{
					AssetDragPayload payload{asset.Handle, asset.Type};
					strncpy_s(payload.Path.data(), payload.Path.size(), asset.Path.c_str(), _TRUNCATE);
					ImGui::SetDragDropPayload(AssetDragPayloadID, &payload, sizeof(payload));
					ImGui::Text("%s  %s", TypeName(asset.Type), name.c_str());
					ImGui::EndDragDropSource();
				}
				if (ImGui::BeginPopupContextItem("##AssetActions"))
				{
					if (asset.Type == Graphics::AssetType::Prefab && context.InstantiatePrefab
						&& ImGui::MenuItem("Instantiate"))
						context.InstantiatePrefab(asset.Handle, asset.Path, 0, nullptr);
					if (asset.Type == Graphics::AssetType::Model && context.InstantiateModel
						&& ImGui::MenuItem("Add to Scene"))
						context.InstantiateModel(asset.Handle, asset.Path, 0, nullptr);
					if (asset.Type == Graphics::AssetType::Prefab && context.DeleteAsset
						&& ImGui::MenuItem("Delete..."))
					{
						m_pendingDelete = asset;
						m_openDeleteConfirmation = true;
					}
					ImGui::EndPopup();
				}
				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%08llx", static_cast<unsigned long long>(asset.Handle & 0xffffffffull));
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
		if (m_openDeleteConfirmation)
		{
			ImGui::OpenPopup("Delete prefab?");
			m_openDeleteConfirmation = false;
		}
		if (ImGui::BeginPopupModal("Delete prefab?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			const std::size_t references = context.CountPrefabReferences
				? context.CountPrefabReferences(m_pendingDelete.Handle) : 0;
			ImGui::Text("Move %s to project trash?",
				std::filesystem::path(m_pendingDelete.Path).filename().string().c_str());
			if (references > 0)
				ImGui::TextColored({0.95f, 0.64f, 0.24f, 1.0f},
					"Referenced by %zu element%s in this scene.",
					references, references == 1 ? "" : "s");
			ImGui::TextDisabled("Ctrl+Z restores the asset and its metadata.");
			if (ImGui::Button("Delete") && context.DeleteAsset)
			{
				context.DeleteAsset(m_pendingDelete.Handle, m_pendingDelete.Path);
				m_pendingDelete = {};
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				m_pendingDelete = {};
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		ImGui::End();
	}
}
