#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include "Layer.h"
#include "../Events/Event.h"
#include "../Graphics/RendererDebug.h"
#include "../Logging/Logging.h"

struct ProfilerMetrics
{
	size_t EntityCount = 0;
	size_t VisibleChunkCount = 0;
	size_t SubmittedInstanceCount = 0;
	double BuildBatchesCpuMs = 0.0;
	double DrawSubmitCpuMs = 0.0;
	double UpdateCpuMs = 0.0;
	double InputLayersCpuMs = 0.0;
	double SimulationCpuMs = 0.0;
	double PhysicsStepCpuMs = 0.0;
	std::size_t PhysicsBodyCount = 0;
	std::size_t PhysicsContactCount = 0;
	std::size_t PhysicsContactEventCount = 0;
	double TransformCpuMs = 0.0;
	std::size_t TransformVisitedCount = 0;
	double CameraCleanupCpuMs = 0.0;
	double RenderWorldCpuMs = 0.0;
	double EditorUiCpuMs = 0.0;
	double PresentCpuMs = 0.0;
	double GpuFrameMs = 0.0;
	std::size_t DrawCallCount = 0;
	std::size_t TriangleCount = 0;
	std::size_t TextureResourceCount = 0;
	std::size_t BufferResourceCount = 0;
	std::uint64_t VideoMemoryUsageBytes = 0;
	std::uint64_t VideoMemoryBudgetBytes = 0;
	bool SceneViewportRendered = false;
	bool GameViewportRendered = false;
	std::size_t JobWorkerCount = 0;
	std::size_t QueuedJobCount = 0;
	std::size_t ActiveJobCount = 0;
	std::size_t PeakQueuedJobCount = 0;
	std::uint64_t CompletedJobCount = 0;
	double AverageJobExecutionMs = 0.0;
	size_t VisibleBatchCount = 0;
	size_t SubmittedDrawCount = 0;
	size_t UploadedInstanceCount = 0;
	size_t UploadedInstanceBytes = 0;
	size_t LargestBatchInstanceCount = 0;
	size_t SplitDrawCount = 0;
	size_t ExhaustedBatchCount = 0;
	size_t SkippedInstanceCount = 0;
	size_t ShadowPassCount = 0;
	size_t ShadowDrawCount = 0;
	size_t ShadowInstanceCount = 0;
	size_t ShadowSplitDrawCount = 0;
	size_t ShadowExhaustedBatchCount = 0;
	size_t ShadowSkippedInstanceCount = 0;
	std::uint8_t ShadowCascadeCount = 0;
	std::size_t ViewportTargetBytes = 0;
	std::size_t ShadowTargetBytes = 0;
};

class DebugLayer : public Layer
{
public:
	using MetricsProvider = std::function<ProfilerMetrics()>;

	explicit DebugLayer(
		MetricsProvider metricsProvider = {},
		RendererDebugSettings* rendererDebugSettings = nullptr);
	~DebugLayer() override;

	void OnAttach() override;
	void OnDetach() override;
	void OnUpdate(float deltaTime) override;
	void OnEvent(Event& e) override;
	void OnImGuiRender() override;

private:
	void SampleMemory();
	static constexpr size_t HistorySize = 180;
	MetricsProvider m_metricsProvider;
	RendererDebugSettings* m_rendererDebugSettings = nullptr;
	std::array<float, HistorySize> m_frameTimeHistory{};
	std::array<float, HistorySize> m_privateMemoryHistory{};
	size_t m_historyOffset = 0;
	size_t m_memoryHistoryOffset = 0;
	float m_smoothedFrameTime = 0.0f;
	float m_minFrameTime = 0.0f;
	float m_maxFrameTime = 0.0f;
	double m_memorySampleAccumulator = 0.0;
	std::uint64_t m_workingSetBytes = 0;
	std::uint64_t m_privateBytes = 0;
	std::uint64_t m_baselinePrivateBytes = 0;
	std::uint64_t m_peakPrivateBytes = 0;
	std::uint64_t m_systemCommitBytes = 0;
	std::uint64_t m_systemCommitLimitBytes = 0;
	std::uint64_t m_availablePhysicalBytes = 0;
	std::uint32_t m_handleCount = 0;
	std::uint32_t m_gdiObjectCount = 0;
	std::uint32_t m_userObjectCount = 0;
	bool m_profilerOpen = true;
};
