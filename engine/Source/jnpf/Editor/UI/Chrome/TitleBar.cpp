#include "Editor/UI/Chrome/TitleBar.h"

#include "Editor/EditorContext.h"
#include "Editor/Commands/CommandStack.h"
#include "Editor/UI/UIScale.h"
#include "Editor/Commands/EditorCommand.h"

#include <imgui.h>

#include <cfloat>

namespace jnpf::Editor
{
	namespace
	{
		const float MenuPopupMinWidth = UI::Scale(184.0f);

		bool BeginChromeMenuPopup(const char* id)
		{
			ImGui::SetNextWindowSizeConstraints(ImVec2(MenuPopupMinWidth, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, UI::Scale(14.0f, 9.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, UI::Scale(10.0f, 6.0f));
			const bool open = ImGui::BeginPopup(id);
			if (!open)
				ImGui::PopStyleVar(2);
			return open;
		}

		void EndChromeMenuPopup()
		{
			ImGui::PopStyleVar(2);
			ImGui::EndPopup();
		}

		std::vector<entt::entity> SelectionRoots(EditorContext& context)
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
				if (!nested) roots.push_back(candidate);
			}
			return roots;
		}

		void DuplicateSelection(EditorContext& context)
		{
			auto command = std::make_unique<CompositeCommand>();
			std::vector<DuplicateEntityCommand*> duplicates;
			for (const entt::entity entity : SelectionRoots(context))
			{
				auto duplicate = std::make_unique<DuplicateEntityCommand>(*context.ActiveScene, entity);
				duplicates.push_back(duplicate.get());
				command->Add(std::move(duplicate));
			}
			if (command->Empty()) return;
			context.Commands->Execute(std::move(command));
			std::vector<entt::entity> selection;
			for (const auto* duplicate : duplicates)
				selection.push_back(context.ActiveScene->FindEntityByID(duplicate->GetEntityID()));
			context.SelectEntities(selection);
		}

		void DeleteSelection(EditorContext& context)
		{
			auto command = std::make_unique<CompositeCommand>();
			for (const entt::entity entity : SelectionRoots(context))
				command->Add(std::make_unique<DeleteEntityCommand>(*context.ActiveScene, entity));
			if (!command->Empty()) context.Commands->Execute(std::move(command));
			context.ClearSelection();
		}
	}

	void TitleBar::Draw(EditorContext& context)
	{
		const auto loadScene = [&context]()
		{
			if (context.LoadScene) context.LoadScene();
		};
		const DirectX::XMUINT2 viewport = context.ViewportProviderCallback();
		const float uiScale = UI::ScaleFactor();
		const float titleBarHeight = 32.0f * uiScale;
		const float captionButtonWidth = 46.0f * uiScale;
		const float captionControlsWidth = captionButtonWidth * 3.0f;
		const ImU32 captionBackground = IM_COL32(18, 18, 20, 255);
		const ImU32 captionText = IM_COL32(224, 222, 216, 255);
		const ImU32 captionGlyph = IM_COL32(245, 245, 245, 255);
		const ImU32 buttonHovered = IM_COL32(44, 43, 42, 255);
		const ImU32 buttonActive = IM_COL32(72, 58, 34, 255);
		const ImU32 closeHovered = IM_COL32(196, 43, 54, 255);
		const ImU32 closeActive = IM_COL32(160, 32, 42, 255);

		ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(static_cast<float>(viewport.x), titleBarHeight), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(1.0f);
		const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus
			| ImGuiWindowFlags_NoNavFocus;
		ImGui::PushStyleColor(ImGuiCol_WindowBg, captionBackground);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		if (ImGui::Begin("##TitleBar", nullptr, flags))
		{
			const ImVec2 barMin = ImGui::GetWindowPos();
			const ImVec2 barMax{
				barMin.x + ImGui::GetWindowWidth() - captionControlsWidth,
				barMin.y + titleBarHeight
			};
			const ImVec2 dragMin{barMin.x + UI::Scale(204.0f), barMin.y};
			ImDrawList* drawList = ImGui::GetWindowDrawList();

			const ImVec2 iconMin{barMin.x + UI::Scale(10.0f), barMin.y + UI::Scale(9.0f)};
			const ImVec2 iconMax{iconMin.x + UI::Scale(14.0f), iconMin.y + UI::Scale(14.0f)};
			drawList->AddRect(iconMin, iconMax, captionText, 0.0f, 1.0f, ImDrawFlags_None);
			drawList->AddLine(
				{iconMin.x + 1.0f, iconMin.y + 4.0f},
				{iconMax.x - 1.0f, iconMin.y + 4.0f},
				captionText);
			drawList->AddText(
				{barMin.x + UI::Scale(32.0f), barMin.y + (titleBarHeight - ImGui::GetFontSize()) * 0.5f},
				captionText,
				context.Commands && context.Commands->IsDirty() ? "jnpf  *" : "jnpf");

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.07f, 0.07f, 0.075f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.215f, 0.205f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.36f, 0.285f, 0.15f, 1.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, UI::Scale(8.0f, 4.0f));
			ImGui::SetCursorScreenPos({barMin.x + UI::Scale(92.0f), barMin.y + UI::Scale(4.0f)});
			if (ImGui::Button("File", UI::Scale(48.0f, 24.0f)))
				ImGui::OpenPopup("##FileMenu");
			if (BeginChromeMenuPopup("##FileMenu"))
			{
				if (ImGui::MenuItem("Save", "Ctrl+S", false, context.SaveScene != nullptr))
				{
					if (context.SaveScene() && context.Commands)
						context.Commands->MarkSaved();
				}
				if (ImGui::MenuItem("Load", "Ctrl+O", false, context.LoadScene != nullptr))
					context.RequestSceneLoad = true;
				ImGui::Separator();
				if (ImGui::MenuItem("Exit") && context.CloseWindow)
					context.RequestCloseEditor = true;
				EndChromeMenuPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Edit", UI::Scale(48.0f, 24.0f)))
				ImGui::OpenPopup("##EditMenu");
			if (BeginChromeMenuPopup("##EditMenu"))
			{
				const bool canUndo = context.Commands && context.Commands->CanUndo();
				const bool canRedo = context.Commands && context.Commands->CanRedo();
				if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo))
					context.Commands->Undo();
				if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canRedo))
					context.Commands->Redo();
				ImGui::Separator();
				const bool canDelete = context.Commands && context.IsSelectedEntityValid();
				if (ImGui::MenuItem("Copy", "Ctrl+C", false, canDelete) && context.CopySelection)
					context.CopySelection();
				if (ImGui::MenuItem("Paste", "Ctrl+V", false, context.PasteSelection != nullptr))
					context.PasteSelection();
				if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, canDelete))
					DuplicateSelection(context);
				if (ImGui::MenuItem("Delete Selected", "Del", false, canDelete))
					DeleteSelection(context);
				EndChromeMenuPopup();
			}
			ImGui::PopStyleVar();
			ImGui::PopStyleColor(3);

			if (ImGui::IsMouseHoveringRect(dragMin, barMax)
				&& ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)
				&& context.ToggleMaximizeWindow)
			{
				context.ToggleMaximizeWindow();
			}
			else if (ImGui::IsMouseHoveringRect(dragMin, barMax)
				&& ImGui::IsMouseClicked(ImGuiMouseButton_Left)
				&& context.BeginTitleBarDrag)
			{
				context.BeginTitleBarDrag();
			}

			const auto drawCaptionButton = [
				drawList,
				captionButtonWidth,
				titleBarHeight,
				captionGlyph
			](
				const char* id,
				const ImVec2 position,
				const ImU32 hoverColor,
				const ImU32 activeColor,
				const auto& drawGlyph)
			{
				ImGui::SetCursorScreenPos(position);
				ImGui::InvisibleButton(id, {captionButtonWidth, titleBarHeight});
				const bool hovered = ImGui::IsItemHovered();
				const bool active = ImGui::IsItemActive();
				if (hovered || active)
				{
					drawList->AddRectFilled(
						position,
						{position.x + captionButtonWidth, position.y + titleBarHeight},
						active ? activeColor : hoverColor);
				}
				drawGlyph(
					{position.x + captionButtonWidth * 0.5f, position.y + titleBarHeight * 0.5f},
					captionGlyph);
				return ImGui::IsItemClicked();
			};

			const float controlsX = barMin.x + ImGui::GetWindowWidth() - captionControlsWidth;
			if (drawCaptionButton(
				"##Minimize",
				{controlsX, barMin.y},
				buttonHovered,
				buttonActive,
				[drawList, uiScale](const ImVec2 center, const ImU32 color)
				{
					drawList->AddLine(
						{center.x - 5.0f * uiScale, center.y + 3.0f * uiScale},
						{center.x + 5.0f * uiScale, center.y + 3.0f * uiScale},
						color);
				}) && context.MinimizeWindow)
			{
				context.MinimizeWindow();
			}

			const bool maximized = context.IsWindowMaximized && context.IsWindowMaximized();
			if (drawCaptionButton(
				"##MaximizeRestore",
				{controlsX + captionButtonWidth, barMin.y},
				buttonHovered,
				buttonActive,
				[drawList, maximized, uiScale](const ImVec2 center, const ImU32 color)
				{
					if (maximized)
					{
						drawList->AddRect(
							{center.x - 3.0f * uiScale, center.y - 5.0f * uiScale},
							{center.x + 5.0f * uiScale, center.y + 3.0f * uiScale},
							color);
						drawList->AddRect(
							{center.x - 5.0f * uiScale, center.y - 3.0f * uiScale},
							{center.x + 3.0f * uiScale, center.y + 5.0f * uiScale},
							color);
					}
					else
					{
						drawList->AddRect(
							{center.x - 5.0f * uiScale, center.y - 5.0f * uiScale},
							{center.x + 5.0f * uiScale, center.y + 5.0f * uiScale},
							color);
					}
				}) && context.ToggleMaximizeWindow)
			{
				context.ToggleMaximizeWindow();
			}

			if (drawCaptionButton(
				"##Close",
				{controlsX + captionButtonWidth * 2.0f, barMin.y},
				closeHovered,
				closeActive,
				[drawList, uiScale](const ImVec2 center, const ImU32 color)
				{
					drawList->AddLine(
						{center.x - 5.0f * uiScale, center.y - 5.0f * uiScale},
						{center.x + 5.0f * uiScale, center.y + 5.0f * uiScale},
						color);
					drawList->AddLine(
						{center.x + 5.0f * uiScale, center.y - 5.0f * uiScale},
						{center.x - 5.0f * uiScale, center.y + 5.0f * uiScale},
						color);
				}) && context.CloseWindow)
			{
				context.RequestCloseEditor = true;
			}
		}
		ImGui::End();
		ImGui::PopStyleVar(3);
		ImGui::PopStyleColor();
		if (context.RequestSceneLoad)
		{
			context.RequestSceneLoad = false;
			if (context.Commands && context.Commands->IsDirty())
				ImGui::OpenPopup("Unsaved scene");
			else
				loadScene();
		}
		if (ImGui::BeginPopupModal("Unsaved scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("The current scene has unsaved command changes.");
			ImGui::TextUnformatted("Save before loading another scene?");
			if (ImGui::Button("Save and load"))
			{
				if (context.SaveScene && context.SaveScene())
				{
					if (context.Commands) context.Commands->MarkSaved();
					loadScene();
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Discard and load"))
			{
				loadScene();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
		if (context.RequestCloseEditor)
		{
			context.RequestCloseEditor = false;
			if (context.Commands && context.Commands->IsDirty())
				ImGui::OpenPopup("Unsaved scene before exit");
			else if (context.CloseWindow)
				context.CloseWindow();
		}
		if (ImGui::BeginPopupModal(
			"Unsaved scene before exit", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("The current scene has unsaved command changes.");
			if (ImGui::Button("Save and exit"))
			{
				if (context.SaveScene && context.SaveScene())
				{
					if (context.Commands) context.Commands->MarkSaved();
					if (context.CloseWindow) context.CloseWindow();
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Discard and exit"))
			{
				if (context.CloseWindow) context.CloseWindow();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
	}
}
