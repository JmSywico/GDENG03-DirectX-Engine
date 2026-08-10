#include "Graphics/Instancing/InstanceRenderer.h"

#include "Core/JobSystem.h"
#include "Graphics/Instancing/InstanceSubmissionPlanner.h"

#include "Graphics/Mesh.h"
#include "Graphics/Model.h"
#include "Graphics/Texture2D.h"
#include "Logging/Logging.h"
#include "Scene/Components/Components.h"

#include <algorithm>
#include <bx/bounds.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <debugdraw/debugdraw.h>

namespace enignE::Graphics
{
	namespace
	{
		constexpr float CameraChunkScale = 0.5f;
		constexpr uint64_t RenderState =
			BGFX_STATE_WRITE_RGB
			| BGFX_STATE_WRITE_A
			| BGFX_STATE_WRITE_Z
			| BGFX_STATE_DEPTH_TEST_LESS
			| BGFX_STATE_CULL_CCW
			| BGFX_STATE_MSAA;

		bx::Aabb MakeAabb(
			const DirectX::XMFLOAT3& boundsMin,
			const DirectX::XMFLOAT3& boundsMax)
		{
			return {
				{boundsMin.x, boundsMin.y, boundsMin.z},
				{boundsMax.x, boundsMax.y, boundsMax.z}
			};
		}

		bx::Aabb ComputeWorldBounds(const Model& model, const DirectX::XMMATRIX& world)
		{
			DirectX::XMFLOAT3 worldMin{
				std::numeric_limits<float>::max(),
				std::numeric_limits<float>::max(),
				std::numeric_limits<float>::max()
			};
			DirectX::XMFLOAT3 worldMax{
				std::numeric_limits<float>::lowest(),
				std::numeric_limits<float>::lowest(),
				std::numeric_limits<float>::lowest()
			};
			bool hasBounds = false;
			for (const auto& mesh : model.GetMeshes())
			{
				if (!mesh)
					continue;
				const DirectX::XMFLOAT3& localMin = mesh->GetBoundsMin();
				const DirectX::XMFLOAT3& localMax = mesh->GetBoundsMax();
				for (uint32_t corner = 0; corner < 8; ++corner)
				{
					const DirectX::XMFLOAT3 local{
						(corner & 1) ? localMax.x : localMin.x,
						(corner & 2) ? localMax.y : localMin.y,
						(corner & 4) ? localMax.z : localMin.z
					};
					DirectX::XMFLOAT3 transformed{};
					DirectX::XMStoreFloat3(
						&transformed,
						DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&local), world));
					worldMin.x = std::min(worldMin.x, transformed.x);
					worldMin.y = std::min(worldMin.y, transformed.y);
					worldMin.z = std::min(worldMin.z, transformed.z);
					worldMax.x = std::max(worldMax.x, transformed.x);
					worldMax.y = std::max(worldMax.y, transformed.y);
					worldMax.z = std::max(worldMax.z, transformed.z);
					hasBounds = true;
				}
			}
			if (!hasBounds)
			{
				DirectX::XMFLOAT4X4 matrix{};
				DirectX::XMStoreFloat4x4(&matrix, world);
				worldMin = {matrix._41, matrix._42, matrix._43};
				worldMax = worldMin;
			}
			return MakeAabb(worldMin, worldMax);
		}

		void DrawBatches(
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
			const ShadowSamplingBindings& shadowBindings,
			bgfx::TextureHandle defaultAlbedoTexture,
			const SceneLightData& lights,
			const std::vector<InstanceBatch>& batches,
			InstanceRendererProfile& profile)
		{
			for (const InstanceBatch& batch : batches)
			{
				if (!batch.IsDrawable())
					continue;

				const uint32_t requestedCount = static_cast<uint32_t>(batch.Instances.size());
				profile.LargestBatchInstanceCount = std::max(
					profile.LargestBatchInstanceCount,
					batch.Instances.size());
				const MaterialResource* material = batch.MaterialResourcePtr.get();
				const DirectX::XMFLOAT4& albedoValue = material ? material->Albedo : batch.Albedo;
				const MaterialMode materialMode = material ? material->Mode : batch.Material;
				const bgfx::TextureHandle albedoTexture =
					material && material->AlbedoTexture && material->AlbedoTexture->IsValid()
					? material->AlbedoTexture->GetHandle()
					: defaultAlbedoTexture;
				const bool hasMetallicRoughnessTexture =
					material
					&& material->MetallicRoughnessTexture
					&& material->MetallicRoughnessTexture->IsValid();
				const bgfx::TextureHandle metallicRoughnessTexture =
					hasMetallicRoughnessTexture
						? material->MetallicRoughnessTexture->GetHandle()
						: defaultAlbedoTexture;
				const bool hasNormalTexture =
					material && material->NormalTexture && material->NormalTexture->IsValid();
				const bgfx::TextureHandle normalTexture = hasNormalTexture
					? material->NormalTexture->GetHandle()
					: defaultAlbedoTexture;
				const float albedo[4] = {
					albedoValue.x,
					albedoValue.y,
					albedoValue.z,
					albedoValue.w
				};
				const float materialModeValue[4] = {
					static_cast<float>(materialMode), 0.0f, 0.0f, 0.0f
				};
				const float materialSurface[4] = {
					material ? std::clamp(material->Metallic, 0.0f, 1.0f) : 0.0f,
					material ? std::clamp(material->Roughness, 0.04f, 1.0f) : 1.0f,
					hasMetallicRoughnessTexture ? 1.0f : 0.0f,
					hasNormalTexture ? 1.0f : 0.0f
				};
				const float materialEmissive[4] = {
					material ? material->Emissive.x : 0.0f,
					material ? material->Emissive.y : 0.0f,
					material ? material->Emissive.z : 0.0f,
					material && material->AlbedoTexture && material->AlbedoTexture->IsValid()
						? 1.0f : 0.0f
				};

				uint32_t instanceOffset = 0;
				while (instanceOffset < requestedCount)
				{
					const uint32_t remainingCount = requestedCount - instanceOffset;
					const uint32_t desiredCount = std::min(
						remainingCount,
						InstanceSubmissionPlanner::MaxInstancesPerSubmission);
					const uint32_t submissionCount = InstanceSubmissionPlanner::NextCount(
						desiredCount,
						bgfx::getAvailInstanceDataBuffer(desiredCount, sizeof(InstanceData)));
					if (submissionCount == 0)
					{
						++profile.ExhaustedBatchCount;
						profile.SkippedInstanceCount += remainingCount;
						LOG_ERRORF(
							"InstanceRenderer::Draw - transient instance buffer exhausted "
							"after submitting {} of {} instances (stride={})",
							instanceOffset,
							requestedCount,
							sizeof(InstanceData));
						break;
					}

					bgfx::InstanceDataBuffer instanceBuffer{};
					bgfx::allocInstanceDataBuffer(&instanceBuffer, submissionCount, sizeof(InstanceData));
					const uint64_t requiredBytes =
						static_cast<uint64_t>(submissionCount) * sizeof(InstanceData);
					if (!instanceBuffer.data
						|| instanceBuffer.num != submissionCount
						|| instanceBuffer.stride != sizeof(InstanceData)
						|| instanceBuffer.size < requiredBytes)
					{
						++profile.ExhaustedBatchCount;
						profile.SkippedInstanceCount += remainingCount;
						LOG_ERRORF(
							"InstanceRenderer::Draw - invalid transient allocation "
							"(requested={}, allocated={}, bytes={}, stride={})",
							submissionCount,
							instanceBuffer.num,
							instanceBuffer.size,
							instanceBuffer.stride);
						break;
					}
					std::memcpy(
						instanceBuffer.data,
						batch.Instances.data() + instanceOffset,
						requiredBytes);

				bgfx::setUniform(albedoUniform, albedo);
				bgfx::setTexture(0, albedoSampler, albedoTexture);
				bgfx::setTexture(1, metallicRoughnessSampler, metallicRoughnessTexture);
				bgfx::setTexture(2, normalSampler, normalTexture);
				const float lightMeta[4] = {
					static_cast<float>(lights.Count),
					static_cast<float>(lights.ShadowLightIndex),
					0.0f,
					0.0f
				};
				bgfx::setUniform(
					lightDirectionsUniform, lights.Directions.data(), SceneLightData::MaxLights);
				bgfx::setUniform(
					lightColorsUniform, lights.Colors.data(), SceneLightData::MaxLights);
				bgfx::setUniform(
					lightPositionsUniform, lights.Positions.data(), SceneLightData::MaxLights);
				bgfx::setUniform(
					lightParametersUniform, lights.Parameters.data(), SceneLightData::MaxLights);
				bgfx::setUniform(lightMetaUniform, lightMeta);
				bgfx::setUniform(materialModeUniform, materialModeValue);
				bgfx::setUniform(materialSurfaceUniform, materialSurface);
				bgfx::setUniform(materialEmissiveUniform, materialEmissive);
				const bool shadowsEnabled = shadowBindings.Enabled
					&& batch.ReceivesShadows
					&& shadowBindings.Frame
					&& shadowBindings.Frame->IsValid();
				const float shadowParameters[4] = {
					shadowsEnabled ? 1.0f : 0.0f,
					shadowBindings.Strength,
					shadowBindings.Bias,
					shadowBindings.NormalBias
				};
				float cascadeSplits[4] = {};
				if (shadowBindings.Frame)
				{
					for (std::uint8_t index = 0; index < ShadowFrameData::MaxCascades; ++index)
						cascadeSplits[index] = shadowBindings.Frame->Views[index].SplitFar;
				}
				const float mapInfo[4] = {
					1.0f / static_cast<float>(ShadowRenderer::MapSize),
					shadowBindings.Frame && shadowBindings.Frame->Directional ? 1.0f : 0.0f,
					shadowBindings.Frame ? static_cast<float>(shadowBindings.Frame->ViewCount) : 0.0f,
					0.1f
				};
				bgfx::setUniform(shadowBindings.Parameters, shadowParameters);
				bgfx::setUniform(shadowBindings.CascadeSplits, cascadeSplits);
				bgfx::setUniform(shadowBindings.MapInfo, mapInfo);
				for (std::uint8_t index = 0; index < ShadowFrameData::MaxCascades; ++index)
				{
					if (shadowBindings.Frame)
						bgfx::setUniform(
							shadowBindings.Matrices[index],
							&shadowBindings.Frame->Views[index].SampleMatrix);
					if (bgfx::isValid(shadowBindings.Textures[index]))
						bgfx::setTexture(
							static_cast<std::uint8_t>(3 + index),
							shadowBindings.Samplers[index],
							shadowBindings.Textures[index]);
				}
				bgfx::setVertexBuffer(0, batch.MeshPtr->GetVertexBuffer());
				if (batch.MeshPtr->HasIndices())
					bgfx::setIndexBuffer(batch.MeshPtr->GetIndexBuffer());
				bgfx::setInstanceDataBuffer(&instanceBuffer);
				bgfx::setState(RenderState);
				bgfx::submit(viewId, program);
				++profile.SubmittedDrawCount;
				if (requestedCount > submissionCount) ++profile.SplitDrawCount;
				profile.UploadedInstanceCount += submissionCount;
				profile.UploadedInstanceBytes += static_cast<size_t>(requiredBytes);
				instanceOffset += submissionCount;
				}
			}
		}

		void DrawShadowBatches(
			bgfx::ViewId viewId,
			bgfx::ProgramHandle program,
			const std::vector<InstanceBatch>& batches,
			InstanceRendererProfile& profile)
		{
			constexpr std::uint64_t ShadowState =
				BGFX_STATE_WRITE_Z
				| BGFX_STATE_DEPTH_TEST_LESS
				| BGFX_STATE_CULL_CCW;
			for (const InstanceBatch& batch : batches)
			{
				if (!batch.IsDrawable())
					continue;
				const std::uint32_t count = static_cast<std::uint32_t>(batch.Instances.size());
				std::uint32_t instanceOffset = 0;
				while (instanceOffset < count)
				{
					const std::uint32_t remainingCount = count - instanceOffset;
					const std::uint32_t desiredCount = std::min(
						remainingCount,
						InstanceSubmissionPlanner::MaxInstancesPerSubmission);
					const std::uint32_t submissionCount = InstanceSubmissionPlanner::NextCount(
						desiredCount,
						bgfx::getAvailInstanceDataBuffer(desiredCount, sizeof(InstanceData)));
					if (submissionCount == 0)
					{
						++profile.ShadowExhaustedBatchCount;
						profile.ShadowSkippedInstanceCount += remainingCount;
						LOG_ERRORF("InstanceRenderer::DrawShadow - transient instance buffer exhausted; skipped {} instances", remainingCount);
						break;
					}
					bgfx::InstanceDataBuffer instanceBuffer{};
					bgfx::allocInstanceDataBuffer(&instanceBuffer, submissionCount, sizeof(InstanceData));
					const std::uint64_t requiredBytes = static_cast<std::uint64_t>(submissionCount) * sizeof(InstanceData);
					if (!instanceBuffer.data || instanceBuffer.num != submissionCount
						|| instanceBuffer.stride != sizeof(InstanceData) || instanceBuffer.size < requiredBytes)
					{
						++profile.ShadowExhaustedBatchCount;
						profile.ShadowSkippedInstanceCount += remainingCount;
						LOG_ERRORF("InstanceRenderer::DrawShadow - invalid transient allocation; skipped {} instances", remainingCount);
						break;
					}
					std::memcpy(
						instanceBuffer.data,
						batch.Instances.data() + instanceOffset,
						submissionCount * sizeof(InstanceData));
				bgfx::setVertexBuffer(0, batch.MeshPtr->GetVertexBuffer());
				if (batch.MeshPtr->HasIndices()) bgfx::setIndexBuffer(batch.MeshPtr->GetIndexBuffer());
				bgfx::setInstanceDataBuffer(&instanceBuffer);
				bgfx::setState(ShadowState);
				bgfx::submit(viewId, program);
				++profile.ShadowDrawCount;
				if (count > submissionCount) ++profile.ShadowSplitDrawCount;
				profile.ShadowInstanceCount += submissionCount;
				instanceOffset += submissionCount;
				}
			}
		}
	}

	void InstanceRenderer::Clear()
	{
		m_chunkManager.Clear();
		m_chunkDrawData.clear();
		m_visibleChunkDrawData.clear();
		m_shadowBatches.clear();
		m_hasCachedCameraChunk = false;
		m_hasCachedViewProjection = false;
		m_lastBuiltRenderVersion = std::numeric_limits<std::uint64_t>::max();
		m_visibleChunkCount = 0;
		m_submittedInstanceCount = 0;
		m_profile = {};
	}

	void InstanceRenderer::BuildBatches(
		entt::registry& registry,
		std::uint64_t sceneRenderVersion,
		const DirectX::XMFLOAT3& cameraPosition,
		const DirectX::XMFLOAT4X4& view,
		const DirectX::XMFLOAT4X4& projection,
		Core::JobSystem* jobs)
	{
		const auto profileStart = std::chrono::steady_clock::now();
		m_cachedCameraPosition = cameraPosition;
		const float cameraCullChunkSize = std::max(1.0f, m_cullDistance * CameraChunkScale);
		ChunkManager cameraChunkManager(cameraCullChunkSize);
		const ChunkCoord cameraChunk = cameraChunkManager.WorldToChunkCoord(cameraPosition);
		const Frustum frustum = BuildFrustum(view, projection);

		const bool renderDataChanged = sceneRenderVersion != m_lastBuiltRenderVersion;
		const bool cameraVisibilityChanged = !m_hasCachedCameraChunk || !(cameraChunk == m_cachedCameraChunk);
		const bool cameraMatrixChanged =
			!m_hasCachedViewProjection
			|| !MatrixNearlyEqual(view, m_cachedView)
			|| !MatrixNearlyEqual(projection, m_cachedProjection);
		if (!renderDataChanged && !cameraVisibilityChanged && !cameraMatrixChanged)
		{
			m_profile.BuildBatchesCpuMs =
				std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - profileStart).count();
			return;
		}

		if (renderDataChanged)
		{
			const std::vector<ChunkCoord> dirtyChunks = m_chunkManager.UpdateFromRegistry(registry, jobs);
			RebuildChunkDrawData(registry, dirtyChunks);
			m_lastBuiltRenderVersion = sceneRenderVersion;
		}

		const auto visibleChunks = m_chunkManager.QueryVisibleChunks(cameraPosition, m_cullDistance, frustum);
		m_visibleChunkDrawData.clear();
		m_visibleChunkDrawData.reserve(visibleChunks.size());
		m_submittedInstanceCount = 0;
		for (const RenderChunk* chunk : visibleChunks)
		{
			auto it = m_chunkDrawData.find(chunk->GetCoord());
			if (it == m_chunkDrawData.end())
				continue;
			m_visibleChunkDrawData.push_back(it->second.get());
			for (const InstanceBatch& batch : it->second->Batches)
				m_submittedInstanceCount += batch.Instances.size();
		}

		m_visibleChunkCount = m_visibleChunkDrawData.size();
		m_profile.VisibleBatchCount = 0;
		for (const ChunkDrawData* chunkData : m_visibleChunkDrawData)
			m_profile.VisibleBatchCount += chunkData->Batches.size();
		m_cachedCameraChunk = cameraChunk;
		m_hasCachedCameraChunk = true;
		m_cachedView = view;
		m_cachedProjection = projection;
		m_hasCachedViewProjection = true;
		m_profile.BuildBatchesCpuMs =
			std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - profileStart).count();
	}

	void InstanceRenderer::DrawDebug(
		bgfx::ViewId viewId,
		entt::registry& registry,
		const RendererDebugSettings& settings) const
	{
		if (!settings.ShowEntityBounds && !settings.ShowChunks && !settings.ShowCulling)
			return;

		DebugDrawEncoder debugDraw;
		debugDraw.begin(viewId, true);
		debugDraw.setWireframe(true);

		if (settings.ShowEntityBounds)
		{
			debugDraw.setColor(0xff00ffff);
			const auto view = registry.view<Scene::MeshRendererComponent, Scene::TransformComponent>();
			for (const entt::entity entity : view)
			{
				const auto& renderer = view.get<Scene::MeshRendererComponent>(entity);
				const auto& transform = view.get<Scene::TransformComponent>(entity);
				if (renderer.bVisible && renderer.ModelPtr)
					debugDraw.draw(ComputeWorldBounds(*renderer.ModelPtr, transform.GetWorldMatrix()));
			}
		}

		if (settings.ShowChunks || settings.ShowCulling)
		{
			const Frustum frustum = BuildFrustum(m_cachedView, m_cachedProjection);
			for (const auto& [coord, chunk] : m_chunkManager.GetChunks())
			{
				UNREFERENCED_PARAMETER(coord);
				if (settings.ShowCulling)
				{
					const bool visible =
						chunk.IsVisibleByDistance(m_cachedCameraPosition, m_cullDistance)
						&& chunk.IntersectsFrustum(frustum);
					debugDraw.setColor(visible ? 0xff40d060 : 0xff4040e0);
				}
				else
				{
					debugDraw.setColor(0xffffa030);
				}
				debugDraw.draw(MakeAabb(chunk.GetBoundsMin(), chunk.GetBoundsMax()));
			}
		}

		debugDraw.end();
	}

	void InstanceRenderer::Draw(
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
		const ShadowSamplingBindings& shadowBindings,
		bgfx::TextureHandle defaultAlbedoTexture,
		const SceneLightData& lights)
	{
		const auto profileStart = std::chrono::steady_clock::now();
		m_profile.DrawSubmitCpuMs = 0.0;
		m_profile.SubmittedDrawCount = 0;
		m_profile.UploadedInstanceCount = 0;
		m_profile.UploadedInstanceBytes = 0;
		m_profile.LargestBatchInstanceCount = 0;
		m_profile.SplitDrawCount = 0;
		m_profile.ExhaustedBatchCount = 0;
		m_profile.SkippedInstanceCount = 0;
		for (ChunkDrawData* chunkData : m_visibleChunkDrawData)
		{
			DrawBatches(
				viewId,
				program,
				albedoUniform,
				albedoSampler,
				metallicRoughnessSampler,
				normalSampler,
				lightDirectionsUniform,
				lightColorsUniform,
				lightPositionsUniform,
				lightParametersUniform,
				lightMetaUniform,
				materialModeUniform,
				materialSurfaceUniform,
				materialEmissiveUniform,
				shadowBindings,
				defaultAlbedoTexture,
				lights,
				chunkData->Batches,
				m_profile);
		}
		m_profile.DrawSubmitCpuMs =
			std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - profileStart).count();
	}

	void InstanceRenderer::DrawShadow(
		bgfx::ViewId viewId,
		bgfx::ProgramHandle program,
		const DirectX::XMFLOAT4X4& view,
		const DirectX::XMFLOAT4X4& projection)
	{
		if (!bgfx::isValid(program))
			return;
		++m_profile.ShadowPassCount;
		const Frustum frustum = BuildFrustum(view, projection);
		for (const auto& [coord, chunk] : m_chunkManager.GetChunks())
		{
			if (!chunk.IntersectsFrustum(frustum))
				continue;
			const auto data = m_chunkDrawData.find(coord);
			if (data != m_chunkDrawData.end())
				DrawShadowBatches(viewId, program, data->second->ShadowBatches, m_profile);
		}
		DrawShadowBatches(viewId, program, m_shadowBatches, m_profile);
	}

	void InstanceRenderer::ResetShadowProfile()
	{
		m_profile.ShadowPassCount = 0;
		m_profile.ShadowDrawCount = 0;
		m_profile.ShadowInstanceCount = 0;
		m_profile.ShadowSplitDrawCount = 0;
		m_profile.ShadowExhaustedBatchCount = 0;
		m_profile.ShadowSkippedInstanceCount = 0;
	}

	bool InstanceRenderer::BatchMatches(const InstanceBatch& batch, const BatchKey& key)
	{
		if (batch.MeshPtr.get() != key.MeshPtr || batch.MaterialResourcePtr != key.MaterialResourcePtr)
			return false;
		if (batch.ReceivesShadows != key.ReceivesShadows)
			return false;
		if (key.MaterialResourcePtr)
			return true;

		return batch.Material == key.Material
			&& batch.Albedo.x == key.Albedo.x
			&& batch.Albedo.y == key.Albedo.y
			&& batch.Albedo.z == key.Albedo.z
			&& batch.Albedo.w == key.Albedo.w;
	}

	InstanceBatch& InstanceRenderer::FindOrCreateBatch(
		std::vector<InstanceBatch>& batches,
		const BatchKey& key,
		const std::shared_ptr<Mesh>& mesh)
	{
		for (InstanceBatch& batch : batches)
		{
			if (BatchMatches(batch, key))
				return batch;
		}
		InstanceBatch batch;
		batch.MeshPtr = mesh;
		batch.Material = key.Material;
		batch.Albedo = key.Albedo;
		batch.MaterialResourcePtr = key.MaterialResourcePtr;
		batch.ReceivesShadows = key.ReceivesShadows;
		batches.push_back(std::move(batch));
		return batches.back();
	}

	void InstanceRenderer::RebuildChunkDrawData(
		entt::registry& registry,
		const std::vector<ChunkCoord>& dirtyChunks)
	{
		m_visibleChunkDrawData.clear();
		m_submittedInstanceCount = 0;

		for (const ChunkCoord& coord : dirtyChunks)
		{
			const auto chunk = m_chunkManager.GetChunks().find(coord);
			if (chunk == m_chunkManager.GetChunks().end())
			{
				m_chunkDrawData.erase(coord);
				continue;
			}

			auto chunkData = std::make_unique<ChunkDrawData>();
			for (const entt::entity entity : chunk->second.GetEntities())
			{
				const auto* renderer = registry.try_get<Scene::MeshRendererComponent>(entity);
				const auto* transform = registry.try_get<Scene::TransformComponent>(entity);
				if (!renderer || !transform || !renderer->bVisible || !renderer->ModelPtr)
					continue;

				const InstanceData instance = InstanceData::FromWorldMatrix(transform->GetWorldMatrix());
				const auto& meshes = renderer->ModelPtr->GetMeshes();
				for (std::size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
				{
					const auto& mesh = meshes[meshIndex];
					if (!mesh)
						continue;
					const auto material = renderer->MaterialResourcePtr
						? renderer->MaterialResourcePtr
						: renderer->ModelPtr->GetMaterial(meshIndex);
					BatchKey key{
						mesh.get(),
						renderer->Material,
						renderer->Albedo,
						material,
						renderer->bReceiveShadows
					};
					InstanceBatch& batch = FindOrCreateBatch(chunkData->Batches, key, mesh);
					batch.Instances.push_back(instance);
					if (renderer->bCastShadows)
					{
						BatchKey shadowKey{mesh.get()};
						FindOrCreateBatch(chunkData->ShadowBatches, shadowKey, mesh)
							.Instances.push_back(instance);
					}
				}
			}
			m_chunkDrawData.insert_or_assign(coord, std::move(chunkData));
		}
	}

	Frustum InstanceRenderer::BuildFrustum(const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection)
	{
		const DirectX::XMMATRIX viewProjection =
			DirectX::XMMatrixMultiply(DirectX::XMLoadFloat4x4(&view), DirectX::XMLoadFloat4x4(&projection));
		DirectX::XMFLOAT4X4 matrix;
		DirectX::XMStoreFloat4x4(&matrix, viewProjection);

		Frustum frustum;
		frustum.Planes[0] = {matrix._14 + matrix._11, matrix._24 + matrix._21, matrix._34 + matrix._31, matrix._44 + matrix._41};
		frustum.Planes[1] = {matrix._14 - matrix._11, matrix._24 - matrix._21, matrix._34 - matrix._31, matrix._44 - matrix._41};
		frustum.Planes[2] = {matrix._14 - matrix._12, matrix._24 - matrix._22, matrix._34 - matrix._32, matrix._44 - matrix._42};
		frustum.Planes[3] = {matrix._14 + matrix._12, matrix._24 + matrix._22, matrix._34 + matrix._32, matrix._44 + matrix._42};
		frustum.Planes[4] = {matrix._13, matrix._23, matrix._33, matrix._43};
		frustum.Planes[5] = {matrix._14 - matrix._13, matrix._24 - matrix._23, matrix._34 - matrix._33, matrix._44 - matrix._43};
		for (DirectX::XMFLOAT4& plane : frustum.Planes)
		{
			const float length = std::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
			if (length > 0.0f)
			{
				plane.x /= length;
				plane.y /= length;
				plane.z /= length;
				plane.w /= length;
			}
		}
		return frustum;
	}

	bool InstanceRenderer::MatrixNearlyEqual(const DirectX::XMFLOAT4X4& lhs, const DirectX::XMFLOAT4X4& rhs)
	{
		constexpr float Epsilon = 0.0001f;
		const float* a = &lhs._11;
		const float* b = &rhs._11;
		for (size_t index = 0; index < 16; ++index)
		{
			if (std::abs(a[index] - b[index]) > Epsilon)
				return false;
		}
		return true;
	}
}
