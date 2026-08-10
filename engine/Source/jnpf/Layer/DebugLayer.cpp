#include "Layer/DebugLayer.h"
#include "Editor/UI/UIScale.h"

#include <imgui.h>
#include <Windows.h>
#include <psapi.h>

#include <algorithm>
#include <cstring>
#include <utility>

DebugLayer::DebugLayer(
	MetricsProvider metricsProvider,
	RendererDebugSettings* rendererDebugSettings)
	: Layer("DebugLayer")
	, m_metricsProvider(std::move(metricsProvider))
	, m_rendererDebugSettings(rendererDebugSettings)
{
}

DebugLayer::~DebugLayer() = default;

void DebugLayer::OnAttach()
{
	SampleMemory();
	LOG_INFO("DebugLayer attached");
}

void DebugLayer::OnDetach()
{
	LOG_INFO("DebugLayer detached");
}

void DebugLayer::OnUpdate(float deltaTime)
{
	const float frameTimeMs = deltaTime * 1000.0f;
	m_frameTimeHistory[m_historyOffset] = frameTimeMs;
	m_historyOffset = (m_historyOffset + 1) % HistorySize;
	m_memorySampleAccumulator += deltaTime;
	if (m_memorySampleAccumulator >= 0.5)
	{
		m_memorySampleAccumulator = 0.0;
		SampleMemory();
	}

	if (m_smoothedFrameTime == 0.0f)
		m_smoothedFrameTime = frameTimeMs;
	else
		m_smoothedFrameTime += (frameTimeMs - m_smoothedFrameTime) * 0.1f;

	m_minFrameTime = frameTimeMs;
	m_maxFrameTime = frameTimeMs;
	for (const float sample : m_frameTimeHistory)
	{
		if (sample <= 0.0f)
			continue;
		m_minFrameTime = std::min(m_minFrameTime, sample);
		m_maxFrameTime = std::max(m_maxFrameTime, sample);
	}
}

void DebugLayer::SampleMemory()
{
	PROCESS_MEMORY_COUNTERS_EX counters{};
	counters.cb = sizeof(counters);
	if (GetProcessMemoryInfo(
		GetCurrentProcess(),
		reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
		sizeof(counters)))
	{
		m_workingSetBytes = counters.WorkingSetSize;
		m_privateBytes = counters.PrivateUsage;
		if (m_baselinePrivateBytes == 0) m_baselinePrivateBytes = m_privateBytes;
		m_peakPrivateBytes = std::max(m_peakPrivateBytes, m_privateBytes);
		m_privateMemoryHistory[m_memoryHistoryOffset] =
			static_cast<float>(m_privateBytes / (1024.0 * 1024.0));
		m_memoryHistoryOffset = (m_memoryHistoryOffset + 1) % HistorySize;
	}
	PERFORMANCE_INFORMATION performance{};
	performance.cb = sizeof(performance);
	if (GetPerformanceInfo(&performance, sizeof(performance)))
	{
		m_systemCommitBytes = performance.CommitTotal * performance.PageSize;
		m_systemCommitLimitBytes = performance.CommitLimit * performance.PageSize;
		m_availablePhysicalBytes = performance.PhysicalAvailable * performance.PageSize;
	}
	DWORD handles = 0;
	if (GetProcessHandleCount(GetCurrentProcess(), &handles)) m_handleCount = handles;
	m_gdiObjectCount = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
	m_userObjectCount = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
}

void DebugLayer::OnEvent(Event& e)
{
	UNREFERENCED_PARAMETER(e);
}

void DebugLayer::OnImGuiRender()
{
	if (!m_profilerOpen)
		return;

	ImGui::SetNextWindowSize(jnpf::Editor::UI::Scale(430.0f, 420.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Stats", &m_profilerOpen, ImGuiWindowFlags_NoFocusOnAppearing))
	{
		ImGui::End();
		return;
	}

	const float fps = m_smoothedFrameTime > 0.0f ? 1000.0f / m_smoothedFrameTime : 0.0f;
	ImGui::Text("Frame %.2f ms  |  %.1f FPS", m_smoothedFrameTime, fps);
	ImGui::Text("Min %.2f ms  |  Max %.2f ms", m_minFrameTime, m_maxFrameTime);
	ImGui::PlotLines(
		"Frame Time",
		m_frameTimeHistory.data(),
		static_cast<int>(m_frameTimeHistory.size()),
		static_cast<int>(m_historyOffset),
		nullptr,
		0.0f,
		std::max(33.0f, m_maxFrameTime * 1.1f),
		jnpf::Editor::UI::Scale(0.0f, 90.0f),
		sizeof(float));

	constexpr double MiB = 1024.0 * 1024.0;
	ImGui::SeparatorText("Process Memory");
	ImGui::Text("Private: %.1f MB  |  Working set: %.1f MB",
		m_privateBytes / MiB, m_workingSetBytes / MiB);
	ImGui::Text("Baseline: %.1f MB  |  Delta: %+.1f MB  |  Peak: %.1f MB",
		m_baselinePrivateBytes / MiB,
		(static_cast<double>(m_privateBytes) - m_baselinePrivateBytes) / MiB,
		m_peakPrivateBytes / MiB);
	ImGui::PlotLines(
		"Private MB", m_privateMemoryHistory.data(),
		static_cast<int>(m_privateMemoryHistory.size()),
		static_cast<int>(m_memoryHistoryOffset), nullptr, 0.0f,
		std::max(128.0f, static_cast<float>(m_peakPrivateBytes / MiB) * 1.1f),
		jnpf::Editor::UI::Scale(0.0f, 54.0f), sizeof(float));
	const double commitPercent = m_systemCommitLimitBytes > 0
		? 100.0 * m_systemCommitBytes / m_systemCommitLimitBytes : 0.0;
	ImGui::Text("System commit: %.1f / %.1f GB (%.1f%%)",
		m_systemCommitBytes / (MiB * 1024.0),
		m_systemCommitLimitBytes / (MiB * 1024.0), commitPercent);
	ImGui::Text("Physical available: %.1f GB  |  Handles: %u",
		m_availablePhysicalBytes / (MiB * 1024.0), m_handleCount);
	ImGui::Text("GUI objects: %u GDI  |  %u USER", m_gdiObjectCount, m_userObjectCount);
	const ProfilerMetrics metrics = m_metricsProvider ? m_metricsProvider() : ProfilerMetrics{};
	if (ImGui::Button("Set memory baseline"))
	{
		m_baselinePrivateBytes = m_privateBytes;
		m_peakPrivateBytes = m_privateBytes;
		m_privateMemoryHistory.fill(0.0f);
		m_memoryHistoryOffset = 0;
	}
	ImGui::SameLine();
	if (ImGui::Button("Log memory snapshot"))
	{
		LOG_INFOF(
			"Memory snapshot: private={:.1f} MB, working={:.1f} MB, delta={:+.1f} MB, "
			"handles={}, GDI={}, USER={}, dx11Video={:.1f}/{:.1f} MB, textures={}, buffers={}",
			m_privateBytes / MiB,
			m_workingSetBytes / MiB,
			(static_cast<double>(m_privateBytes) - m_baselinePrivateBytes) / MiB,
			m_handleCount,
			m_gdiObjectCount,
			m_userObjectCount,
			static_cast<double>(metrics.VideoMemoryUsageBytes) / MiB,
			static_cast<double>(metrics.VideoMemoryBudgetBytes) / MiB,
			metrics.TextureResourceCount,
			metrics.BufferResourceCount);
	}

	ImGui::SeparatorText("Scene");
	ImGui::Text("Elements: %zu", metrics.EntityCount);
	ImGui::Text("Visible chunks: %zu", metrics.VisibleChunkCount);
	ImGui::Text("Submitted instances: %zu", metrics.SubmittedInstanceCount);

	ImGui::SeparatorText("CPU Frame Breakdown");
	ImGui::Text("Update total: %.3f ms", metrics.UpdateCpuMs);
	ImGui::Text("Input + layers: %.3f ms", metrics.InputLayersCpuMs);
	ImGui::Text("Simulation: %.3f ms", metrics.SimulationCpuMs);
	ImGui::Text("Physics: %.3f ms  |  %zu bodies  |  %zu contacts",
		metrics.PhysicsStepCpuMs, metrics.PhysicsBodyCount, metrics.PhysicsContactCount);
	ImGui::TextDisabled("Contact events this step: %zu", metrics.PhysicsContactEventCount);
	ImGui::Text("Transform propagation: %.3f ms  |  %zu visited",
		metrics.TransformCpuMs, metrics.TransformVisitedCount);
	ImGui::Text("Camera + cleanup: %.3f ms", metrics.CameraCleanupCpuMs);
	ImGui::Text("World rendering: %.3f ms", metrics.RenderWorldCpuMs);
	ImGui::Text("Editor UI: %.3f ms", metrics.EditorUiCpuMs);
	ImGui::Text("DX11 present: %.3f ms", metrics.PresentCpuMs);
	ImGui::Text("Rendered viewports: Scene %s  |  Game %s",
		metrics.SceneViewportRendered ? "on" : "off",
		metrics.GameViewportRendered ? "on" : "off");

	ImGui::SeparatorText("Job Fabric");
	ImGui::Text("Workers: %zu  |  Active: %zu  |  Queued: %zu",
		metrics.JobWorkerCount, metrics.ActiveJobCount, metrics.QueuedJobCount);
	ImGui::Text("Completed: %llu  |  Peak queue: %zu  |  Mean: %.3f ms",
		static_cast<unsigned long long>(metrics.CompletedJobCount),
		metrics.PeakQueuedJobCount,
		metrics.AverageJobExecutionMs);

	ImGui::SeparatorText("Batching");
	ImGui::Text("Build batches CPU: %.3f ms", metrics.BuildBatchesCpuMs);
	ImGui::Text("Draw submission CPU: %.3f ms", metrics.DrawSubmitCpuMs);
	ImGui::Text("Visible batches: %zu", metrics.VisibleBatchCount);
	ImGui::Text(
		"Submitted draws / instances: %zu / %zu",
		metrics.SubmittedDrawCount,
		metrics.UploadedInstanceCount);
	ImGui::Text(
		"Instance upload: %.2f KB  |  Largest batch: %zu",
		static_cast<double>(metrics.UploadedInstanceBytes) / 1024.0,
		metrics.LargestBatchInstanceCount);
	ImGui::Text("Capacity-driven split draws: %zu", metrics.SplitDrawCount);
	if (metrics.ExhaustedBatchCount > 0)
	{
		ImGui::TextColored(
			ImVec4(1.0f, 0.35f, 0.25f, 1.0f),
			"Transient exhaustion: %zu batches, %zu instances skipped",
			metrics.ExhaustedBatchCount,
			metrics.SkippedInstanceCount);
	}

	ImGui::SeparatorText("Shadows");
	ImGui::Text(
		"2048 px D16  |  %u cascades  |  3x3 PCF",
		static_cast<unsigned>(metrics.ShadowCascadeCount));
	ImGui::Text(
		"Passes / draws / casters: %zu / %zu / %zu",
		metrics.ShadowPassCount,
		metrics.ShadowDrawCount,
		metrics.ShadowInstanceCount);
	ImGui::Text("Capacity-driven shadow split draws: %zu", metrics.ShadowSplitDrawCount);
	if (metrics.ShadowExhaustedBatchCount > 0)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f),
			"Shadow transient exhaustion: %zu batches, %zu instances skipped",
			metrics.ShadowExhaustedBatchCount, metrics.ShadowSkippedInstanceCount);
	}
	ImGui::Text("Estimated shadow targets: %.1f MB", metrics.ShadowTargetBytes / MiB);
	ImGui::Text("GPU frame: %.3f ms", metrics.GpuFrameMs);

	if (m_rendererDebugSettings)
	{
		ImGui::SeparatorText("Debug Views");
		ImGui::Checkbox("Element bounds", &m_rendererDebugSettings->ShowEntityBounds);
		ImGui::Checkbox("Chunk bounds", &m_rendererDebugSettings->ShowChunks);
		ImGui::Checkbox("Culling result", &m_rendererDebugSettings->ShowCulling);
		ImGui::Checkbox("World normals", &m_rendererDebugSettings->ShowNormals);
		ImGui::Checkbox("Physics colliders", &m_rendererDebugSettings->ShowPhysicsColliders);
		ImGui::Checkbox("Physics contacts", &m_rendererDebugSettings->ShowPhysicsContacts);
		ImGui::TextDisabled("Culling: green visible, red rejected");
		ImGui::TextDisabled("Physics: cyan colliders, magenta contact normals");
	}

	ImGui::SeparatorText("DX11 Renderer");
	ImGui::Text("Draw calls: %zu  |  Triangles: %zu",
		metrics.DrawCallCount, metrics.TriangleCount);
	ImGui::Text("Textures: %zu  |  Buffers: %zu",
		metrics.TextureResourceCount, metrics.BufferResourceCount);
	ImGui::Text("Video memory: %.1f / %.1f MB",
		static_cast<double>(metrics.VideoMemoryUsageBytes) / MiB,
		static_cast<double>(metrics.VideoMemoryBudgetBytes) / MiB);
	ImGui::Text("Viewport target estimate: %.1f MB", metrics.ViewportTargetBytes / MiB);

	ImGui::End();
}
