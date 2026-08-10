#pragma once

#include "ChunkManager.h"
#include "InstanceBatch.h"
#include "../RendererDebug.h"
#include "../ShadowRenderer.h"

#include <bgfx/bgfx.h>
#include <entt/entity/registry.hpp>

#include <cstdint>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

namespace enignE::Core { class JobSystem; }

namespace enignE::Graphics
{
	/** GPU-ready, bounded light list shared by every scene draw in a frame. */
	struct SceneLightData
	{
		static constexpr std::uint16_t MaxLights = 16;

		std::array<DirectX::XMFLOAT4, MaxLights> Directions{};
		std::array<DirectX::XMFLOAT4, MaxLights> Colors{};
		std::array<DirectX::XMFLOAT4, MaxLights> Positions{};
		std::array<DirectX::XMFLOAT4, MaxLights> Parameters{};
		std::uint16_t Count = 0;
		int ShadowLightIndex = -1;
	};

	struct ShadowSamplingBindings
	{
		std::array<bgfx::UniformHandle, ShadowFrameData::MaxCascades> Samplers{};
		std::array<bgfx::UniformHandle, ShadowFrameData::MaxCascades> Matrices{};
		bgfx::UniformHandle Parameters = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle CascadeSplits = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle MapInfo = BGFX_INVALID_HANDLE;
		std::array<bgfx::TextureHandle, ShadowFrameData::MaxCascades> Textures{};
		const ShadowFrameData* Frame = nullptr;
		float Strength = 1.0f;
		float Bias = 0.0012f;
		float NormalBias = 0.02f;
		bool Enabled = false;
	};

	/** @brief Per-frame CPU and submission metrics for instanced rendering. @ingroup rendering */
	struct InstanceRendererProfile
	{
		double BuildBatchesCpuMs = 0.0;
		double DrawSubmitCpuMs = 0.0;
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
	};

	/**
	 * @brief Builds spatially chunked instance batches and submits bgfx draws.
	 * @ingroup rendering
	 *
	 * BuildBatches compares the supplied scene render version with the last
	 * completed build. Chunk membership and cached draw data are rebuilt only
	 * where ChunkManager reports changed render state.
	 */
	class InstanceRenderer
	{
	public:
		void SetChunkSize(float chunkSize)
		{
			m_chunkManager.SetChunkSize(chunkSize);
			m_chunkDrawData.clear();
			m_visibleChunkDrawData.clear();
			m_visibleChunkCount = 0;
			m_submittedInstanceCount = 0;
			m_lastBuiltRenderVersion = std::numeric_limits<std::uint64_t>::max();
		}
		float GetChunkSize() const { return m_chunkManager.GetChunkSize(); }
		void SetCullDistance(float distance)
		{
			m_cullDistance = distance;
			m_hasCachedCameraChunk = false;
		}
		float GetCullDistance() const { return m_cullDistance; }
		void Clear();

		/**
		 * @brief Synchronizes chunk caches and computes the visible batch set.
		 * @param registry Registry containing transform and mesh-renderer components.
		 * @param sceneRenderVersion Current Scene::GetRenderVersion() value.
		 * @param cameraPosition World-space camera position used for distance culling.
		 * @param view Current view matrix.
		 * @param projection Current projection matrix.
		 */
		void BuildBatches(
			entt::registry& registry,
			std::uint64_t sceneRenderVersion,
			const DirectX::XMFLOAT3& cameraPosition,
			const DirectX::XMFLOAT4X4& view,
			const DirectX::XMFLOAT4X4& projection,
			Core::JobSystem* jobs = nullptr);
		/**
		 * @brief Uploads transient instance data and submits one draw per visible batch.
		 * Large batches are split across the transient instance-buffer space that
		 * remains. Only instances for which no space remains are skipped.
		 */
		void Draw(
			bgfx::ViewId viewId,
			bgfx::ProgramHandle program,
			bgfx::UniformHandle albedoUniform,
			bgfx::UniformHandle albedoSampler,
			bgfx::UniformHandle metallicRoughnessSampler,
			bgfx::UniformHandle normalSampler,
			bgfx::UniformHandle lightDirectionsUniform,
			bgfx::UniformHandle lightColorsUniform,
			bgfx::UniformHandle lightPositionsUniform,
			bgfx::UniformHandle lightParametersUniform,
			bgfx::UniformHandle lightMetaUniform,
			bgfx::UniformHandle materialModeUniform,
			bgfx::UniformHandle materialSurfaceUniform,
			bgfx::UniformHandle materialEmissiveUniform,
			const struct ShadowSamplingBindings& shadowBindings,
			bgfx::TextureHandle defaultAlbedoTexture,
			const SceneLightData& lights);
		void DrawShadow(
			bgfx::ViewId viewId,
			bgfx::ProgramHandle program,
			const DirectX::XMFLOAT4X4& view,
			const DirectX::XMFLOAT4X4& projection);
		void ResetShadowProfile();
		void DrawDebug(
			bgfx::ViewId viewId,
			entt::registry& registry,
			const RendererDebugSettings& settings) const;

		size_t GetVisibleChunkCount() const { return m_visibleChunkCount; }
		size_t GetSubmittedInstanceCount() const { return m_submittedInstanceCount; }
		std::uint64_t GetLastBuiltRenderVersion() const { return m_lastBuiltRenderVersion; }
		const InstanceRendererProfile& GetProfile() const { return m_profile; }

	private:
		struct BatchKey
		{
			const Mesh* MeshPtr = nullptr;
			MaterialMode Material = MaterialMode::LitTint;
			DirectX::XMFLOAT4 Albedo = {1.0f, 1.0f, 1.0f, 1.0f};
			std::shared_ptr<MaterialResource> MaterialResourcePtr;
			bool ReceivesShadows = true;
		};

		static bool BatchMatches(const InstanceBatch& batch, const BatchKey& key);
		static InstanceBatch& FindOrCreateBatch(std::vector<InstanceBatch>& batches, const BatchKey& key, const std::shared_ptr<Mesh>& mesh);
		void RebuildChunkDrawData(entt::registry& registry, const std::vector<ChunkCoord>& dirtyChunks);
		static Frustum BuildFrustum(const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection);
		static bool MatrixNearlyEqual(const DirectX::XMFLOAT4X4& lhs, const DirectX::XMFLOAT4X4& rhs);

	public:
		struct ChunkDrawData
		{
			std::vector<InstanceBatch> Batches;
			std::vector<InstanceBatch> ShadowBatches;
		};

	private:
		ChunkManager m_chunkManager;
		std::unordered_map<ChunkCoord, std::unique_ptr<ChunkDrawData>, ChunkCoordHash> m_chunkDrawData;
		std::vector<ChunkDrawData*> m_visibleChunkDrawData;

		std::vector<InstanceBatch> m_shadowBatches;
		bool m_hasCachedCameraChunk = false;
		bool m_hasCachedViewProjection = false;
		ChunkCoord m_cachedCameraChunk;
		DirectX::XMFLOAT4X4 m_cachedView;
		DirectX::XMFLOAT4X4 m_cachedProjection;
		DirectX::XMFLOAT3 m_cachedCameraPosition = {0.0f, 0.0f, 0.0f};
		std::uint64_t m_lastBuiltRenderVersion = std::numeric_limits<std::uint64_t>::max();
		float m_cullDistance = 250.0f;
		size_t m_visibleChunkCount = 0;
		size_t m_submittedInstanceCount = 0;
		InstanceRendererProfile m_profile;
	};
}
