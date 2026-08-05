#include "Editor/UI/Widgets/GizmoToolbar.h"

#include "Editor/EditorContext.h"
#include "Editor/UI/UIScale.h"

#include <imgui.h>

namespace enignE::Editor
{
	namespace
	{
		bool DrawToolButton(const char* id, bool active, const char* tooltip, const auto& drawIcon)
		{
			const ImVec2 size = UI::Scale(30.0f, 30.0f);
			if (active)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.38f, 0.30f, 0.155f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.46f, 0.36f, 0.18f, 1.0f));
			}
			const bool pressed = ImGui::Button(id, size);
			const ImVec2 min = ImGui::GetItemRectMin();
			const ImVec2 max = ImGui::GetItemRectMax();
			const ImU32 color = active
				? IM_COL32(245, 194, 105, 255)
				: IM_COL32(205, 215, 210, 255);
			drawIcon(ImGui::GetWindowDrawList(), min, max, color);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", tooltip);
			if (active)
				ImGui::PopStyleColor(2);
			return pressed;
		}
	}

	void GizmoToolbar::Draw(
		EditorContext& context,
		const DirectX::XMFLOAT2& sceneViewportOrigin,
		const DirectX::XMUINT2& sceneViewportSize)
	{
		if (sceneViewportSize.x == 0 || sceneViewportSize.y == 0)
			return;

		ImGui::SetNextWindowPos(
			ImVec2(sceneViewportOrigin.x + UI::Scale(12.0f), sceneViewportOrigin.y + UI::Scale(12.0f)),
			ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.78f);
		const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
			| ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_AlwaysAutoResize
			| ImGuiWindowFlags_NoSavedSettings
			| ImGuiWindowFlags_NoDocking;
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, UI::Scale(5.0f, 5.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, UI::Scale(4.0f, 0.0f));
		if (ImGui::Begin("##ViewportTools", nullptr, flags))
		{
			if (DrawToolButton(
				"##MoveTool",
				context.CurrentGizmoMode == GizmoMode::Translate,
				"Move [W]",
				[](ImDrawList* drawList, const ImVec2 min, const ImVec2 max, const ImU32 color)
				{
					const ImVec2 c{(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f};
					drawList->AddLine({c.x - 8.0f, c.y}, {c.x + 8.0f, c.y}, color, 2.0f);
					drawList->AddLine({c.x, c.y - 8.0f}, {c.x, c.y + 8.0f}, color, 2.0f);
					drawList->AddTriangleFilled({c.x + 8.0f, c.y}, {c.x + 4.0f, c.y - 3.0f}, {c.x + 4.0f, c.y + 3.0f}, color);
					drawList->AddTriangleFilled({c.x, c.y - 8.0f}, {c.x - 3.0f, c.y - 4.0f}, {c.x + 3.0f, c.y - 4.0f}, color);
				}))
			{
				context.CurrentGizmoMode = GizmoMode::Translate;
			}
			ImGui::SameLine();
			if (DrawToolButton(
				"##RotateTool",
				context.CurrentGizmoMode == GizmoMode::Rotate,
				"Rotate [E]",
				[](ImDrawList* drawList, const ImVec2 min, const ImVec2 max, const ImU32 color)
				{
					const ImVec2 c{(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f};
					drawList->AddCircle(c, 8.0f, color, 24, 2.0f);
					drawList->AddTriangleFilled({c.x + 8.0f, c.y - 4.0f}, {c.x + 12.0f, c.y - 4.0f}, {c.x + 9.0f, c.y}, color);
				}))
			{
				context.CurrentGizmoMode = GizmoMode::Rotate;
			}
			ImGui::SameLine();
			if (DrawToolButton(
				"##ScaleTool",
				context.CurrentGizmoMode == GizmoMode::Scale,
				"Scale [R]",
				[](ImDrawList* drawList, const ImVec2 min, const ImVec2 max, const ImU32 color)
				{
					const ImVec2 c{(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f};
					drawList->AddRect({c.x - 7.0f, c.y - 7.0f}, {c.x + 7.0f, c.y + 7.0f}, color, 1.5f, 0, 2.0f);
					drawList->AddLine({c.x - 10.0f, c.y + 10.0f}, {c.x - 4.0f, c.y + 4.0f}, color, 2.0f);
					drawList->AddLine({c.x + 4.0f, c.y - 4.0f}, {c.x + 10.0f, c.y - 10.0f}, color, 2.0f);
				}))
			{
				context.CurrentGizmoMode = GizmoMode::Scale;
			}

			const SimulationState simulation = context.GetSimulationState
				? context.GetSimulationState() : SimulationState::Stopped;
			ImGui::SameLine();
			ImGui::TextDisabled("|");
			ImGui::SameLine();
			if (simulation == SimulationState::Stopped)
			{
				if (ImGui::Button("Run", UI::Scale(42.0f, 30.0f)) && context.BeginPlay)
					context.BeginPlay();
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Run an isolated copy of the editor scene");
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.38f, 0.18f, 0.12f, 1.0f));
				if (ImGui::Button("Stop", UI::Scale(42.0f, 30.0f)) && context.StopPlay)
					context.StopPlay();
				ImGui::PopStyleColor();
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop and discard runtime scene changes");
				ImGui::SameLine();
				const char* pauseLabel = simulation == SimulationState::Paused ? "Resume" : "Pause";
				if (ImGui::Button(pauseLabel, UI::Scale(58.0f, 30.0f)) && context.TogglePause)
					context.TogglePause();
				if (simulation == SimulationState::Paused)
				{
					ImGui::SameLine();
					if (ImGui::Button("Step", UI::Scale(42.0f, 30.0f)) && context.StepSimulation)
						context.StepSimulation();
				}
			}
		}
		ImGui::End();
		ImGui::PopStyleVar(2);
	}
}
