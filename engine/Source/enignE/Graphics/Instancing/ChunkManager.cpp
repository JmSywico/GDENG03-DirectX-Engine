#include "Graphics/Instancing/ChunkManager.h"

#include "Core/JobSystem.h"
#include "Scene/Components/Components.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace enignE::Graphics
{
	namespace
	{
		struct Bounds
		{
			DirectX::XMFLOAT3 Min = {0.0f, 0.0f, 0.0f};
			DirectX::XMFLOAT3 Max = {0.0f, 0.0f, 0.0f};
		};

		void ExpandBounds(Bounds& bounds, const DirectX::XMFLOAT3& point)
		{
			bounds.Min.x = std::min(bounds.Min.x, point.x);
			bounds.Min.y = std::min(bounds.Min.y, point.y);
			bounds.Min.z = std::min(bounds.Min.z, point.z);
			bounds.Max.x = std::max(bounds.Max.x, point.x);
			bounds.Max.y = std::max(bounds.Max.y, point.y);
			bounds.Max.z = std::max(bounds.Max.z, point.z);
		}

		Bounds ComputeWorldBounds(const Model& model, const DirectX::XMMATRIX& world)
		{
			Bounds bounds;
			bounds.Min = {
				std::numeric_limits<float>::max(),
				std::numeric_limits<float>::max(),
				std::numeric_limits<float>::max()
			};
			bounds.Max = {
				std::numeric_limits<float>::lowest(),
				std::numeric_limits<float>::lowest(),
				std::numeric_limits<float>::lowest()
			};

			bool hasBounds = false;
			for (const auto& mesh : model.GetMeshes())
			{
				if (!mesh)
					continue;

				const DirectX::XMFLOAT3 localMin = mesh->GetBoundsMin();
				const DirectX::XMFLOAT3 localMax = mesh->GetBoundsMax();
				const DirectX::XMFLOAT3 corners[8] = {
					{localMin.x, localMin.y, localMin.z},
					{localMax.x, localMin.y, localMin.z},
					{localMin.x, localMax.y, localMin.z},
					{localMax.x, localMax.y, localMin.z},
					{localMin.x, localMin.y, localMax.z},
					{localMax.x, localMin.y, localMax.z},
					{localMin.x, localMax.y, localMax.z},
					{localMax.x, localMax.y, localMax.z}
				};

				for (const DirectX::XMFLOAT3& corner : corners)
				{
					DirectX::XMFLOAT3 transformed;
					DirectX::XMStoreFloat3(
						&transformed,
						DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&corner), world));

					if (!hasBounds)
					{
						bounds.Min = transformed;
						bounds.Max = transformed;
						hasBounds = true;
					}
					else
					{
						ExpandBounds(bounds, transformed);
					}
				}
			}

			if (!hasBounds)
			{
				DirectX::XMFLOAT4X4 matrix{};
				DirectX::XMStoreFloat4x4(&matrix, world);
				bounds.Min = {matrix._41, matrix._42, matrix._43};
				bounds.Max = bounds.Min;
			}

			return bounds;
		}
	}

	ChunkManager::ChunkManager(float chunkSize)
	{
		SetChunkSize(chunkSize);
	}

	void ChunkManager::SetChunkSize(float chunkSize)
	{
		m_chunkSize = std::max(1.0f, chunkSize);
		Clear();
	}

	void ChunkManager::Clear()
	{
		m_chunks.clear();
		m_entityStates.clear();
	}

	ChunkCoord ChunkManager::WorldToChunkCoord(const DirectX::XMFLOAT3& worldPosition) const
	{
		return {
			static_cast<int32_t>(std::floor(worldPosition.x / m_chunkSize)),
			static_cast<int32_t>(std::floor(worldPosition.y / m_chunkSize)),
			static_cast<int32_t>(std::floor(worldPosition.z / m_chunkSize))
		};
	}

	std::vector<ChunkCoord> ChunkManager::UpdateFromRegistry(
		entt::registry& registry,
		Core::JobSystem* jobs)
	{
		std::unordered_map<entt::entity, EntityRenderState> currentStates;
		std::unordered_set<ChunkCoord, ChunkCoordHash> dirtyChunks;

		struct RenderInput
		{
			entt::entity Entity = entt::null;
			std::shared_ptr<Model> ModelPtr;
			std::shared_ptr<MaterialResource> MaterialPtr;
			DirectX::XMFLOAT4X4 World{};
			DirectX::XMFLOAT4 Albedo{};
			std::uint8_t Material = 0;
			bool CastShadows = true;
			bool ReceiveShadows = true;
		};
		std::vector<RenderInput> inputs;
		auto view = registry.view<Scene::MeshRendererComponent, Scene::TransformComponent>();
		inputs.reserve(view.size_hint());
		for (const entt::entity entity : view)
		{
			const auto& renderer = view.get<Scene::MeshRendererComponent>(entity);
			const auto& transform = view.get<Scene::TransformComponent>(entity);
			if (!renderer.bVisible || !renderer.ModelPtr)
				continue;
			DirectX::XMFLOAT4X4 world{};
			DirectX::XMStoreFloat4x4(&world, transform.GetWorldMatrix());
			inputs.push_back({
				.Entity = entity,
				.ModelPtr = renderer.ModelPtr,
				.MaterialPtr = renderer.MaterialResourcePtr,
				.World = world,
				.Albedo = renderer.Albedo,
				.Material = static_cast<std::uint8_t>(renderer.Material),
				.CastShadows = renderer.bCastShadows,
				.ReceiveShadows = renderer.bReceiveShadows
			});
		}

		std::vector<EntityRenderState> preparedStates(inputs.size());
		const auto prepare = [this, &inputs, &preparedStates](std::size_t index)
		{
			const RenderInput& input = inputs[index];
			const DirectX::XMMATRIX worldMatrix = DirectX::XMLoadFloat4x4(&input.World);
			const Bounds bounds = ComputeWorldBounds(*input.ModelPtr, worldMatrix);
			const MaterialResource* material = input.MaterialPtr.get();
			preparedStates[index] = EntityRenderState{
				.Coord = WorldToChunkCoord({input.World._41, input.World._42, input.World._43}),
				.BoundsMin = bounds.Min,
				.BoundsMax = bounds.Max,
				.World = input.World,
				.ModelPtr = input.ModelPtr.get(),
				.MaterialResourcePtr = material,
				.MaterialTexturePtr = material
					? material->AlbedoTexture.get()
					: nullptr,
				.NormalTexturePtr = material
					? material->NormalTexture.get()
					: nullptr,
				.MetallicRoughnessTexturePtr = material
					? material->MetallicRoughnessTexture.get()
					: nullptr,
				.MaterialAlbedo = material
					? material->Albedo
					: DirectX::XMFLOAT4{1.0f, 1.0f, 1.0f, 1.0f},
				.MaterialEmissive = material
					? material->Emissive
					: DirectX::XMFLOAT3{},
				.Metallic = material
					? material->Metallic
					: 0.0f,
				.Roughness = material
					? material->Roughness
					: 1.0f,
				.MaterialResourceMode = material
					? static_cast<std::uint8_t>(material->Mode)
					: std::uint8_t{0},
				.Albedo = input.Albedo,
				.Material = input.Material,
				.CastShadows = input.CastShadows,
				.ReceiveShadows = input.ReceiveShadows
			};
		};
		if (jobs && inputs.size() >= 512)
			jobs->ParallelFor(inputs.size(), 128, prepare);
		else
			for (std::size_t index = 0; index < inputs.size(); ++index) prepare(index);

		currentStates.reserve(inputs.size());
		for (std::size_t index = 0; index < inputs.size(); ++index)
		{
			const entt::entity entity = inputs[index].Entity;
			EntityRenderState& state = preparedStates[index];
			const auto previous = m_entityStates.find(entity);
			if (previous == m_entityStates.end())
			{
				dirtyChunks.insert(state.Coord);
			}
			else if (!RenderStateMatches(previous->second, state))
			{
				dirtyChunks.insert(previous->second.Coord);
				dirtyChunks.insert(state.Coord);
			}
			currentStates.emplace(entity, state);
		}

		for (const auto& [entity, previous] : m_entityStates)
		{
			if (!currentStates.contains(entity))
				dirtyChunks.insert(previous.Coord);
		}

		for (const ChunkCoord& coord : dirtyChunks)
		{
			auto chunk = m_chunks.find(coord);
			if (chunk == m_chunks.end())
				chunk = m_chunks.emplace(coord, RenderChunk(coord, m_chunkSize)).first;
			else
				chunk->second.ClearEntities();
		}

		for (const auto& [entity, state] : currentStates)
		{
			if (dirtyChunks.contains(state.Coord))
				AddEntityToChunk(entity, {state.World._41, state.World._42, state.World._43}, state.BoundsMin, state.BoundsMax);
		}

		for (const ChunkCoord& coord : dirtyChunks)
		{
			const auto chunk = m_chunks.find(coord);
			if (chunk != m_chunks.end() && chunk->second.IsEmpty())
				m_chunks.erase(chunk);
		}

		m_entityStates = std::move(currentStates);
		return {dirtyChunks.begin(), dirtyChunks.end()};
	}

	std::vector<const RenderChunk*> ChunkManager::QueryVisibleChunks(
		const DirectX::XMFLOAT3& cameraPosition,
		float maxDistance,
		const Frustum& frustum) const
	{
		std::vector<const RenderChunk*> visible;
		visible.reserve(m_chunks.size());

		for (const auto& [coord, chunk] : m_chunks)
		{
			if (chunk.IsVisibleByDistance(cameraPosition, maxDistance) && chunk.IntersectsFrustum(frustum))
				visible.push_back(&chunk);
		}

		return visible;
	}

	void ChunkManager::AddEntityToChunk(
		entt::entity entity,
		const DirectX::XMFLOAT3& worldPosition,
		const DirectX::XMFLOAT3& boundsMin,
		const DirectX::XMFLOAT3& boundsMax)
	{
		const ChunkCoord coord = WorldToChunkCoord(worldPosition);
		auto it = m_chunks.find(coord);
		if (it == m_chunks.end())
			it = m_chunks.emplace(coord, RenderChunk(coord, m_chunkSize)).first;

		it->second.AddEntity(entity, boundsMin, boundsMax);
	}

	bool ChunkManager::RenderStateMatches(const EntityRenderState& lhs, const EntityRenderState& rhs)
	{
		const float* lhsWorld = &lhs.World._11;
		const float* rhsWorld = &rhs.World._11;
		for (size_t index = 0; index < 16; ++index)
		{
			if (lhsWorld[index] != rhsWorld[index])
				return false;
		}

		const bool commonStateMatches = lhs.Coord == rhs.Coord
			&& lhs.BoundsMin.x == rhs.BoundsMin.x
			&& lhs.BoundsMin.y == rhs.BoundsMin.y
			&& lhs.BoundsMin.z == rhs.BoundsMin.z
			&& lhs.BoundsMax.x == rhs.BoundsMax.x
			&& lhs.BoundsMax.y == rhs.BoundsMax.y
			&& lhs.BoundsMax.z == rhs.BoundsMax.z
			&& lhs.ModelPtr == rhs.ModelPtr
			&& lhs.MaterialResourcePtr == rhs.MaterialResourcePtr
			&& lhs.MaterialTexturePtr == rhs.MaterialTexturePtr
			&& lhs.NormalTexturePtr == rhs.NormalTexturePtr
			&& lhs.MetallicRoughnessTexturePtr == rhs.MetallicRoughnessTexturePtr
			&& lhs.MaterialAlbedo.x == rhs.MaterialAlbedo.x
			&& lhs.MaterialAlbedo.y == rhs.MaterialAlbedo.y
			&& lhs.MaterialAlbedo.z == rhs.MaterialAlbedo.z
			&& lhs.MaterialAlbedo.w == rhs.MaterialAlbedo.w
			&& lhs.MaterialEmissive.x == rhs.MaterialEmissive.x
			&& lhs.MaterialEmissive.y == rhs.MaterialEmissive.y
			&& lhs.MaterialEmissive.z == rhs.MaterialEmissive.z
			&& lhs.Metallic == rhs.Metallic
			&& lhs.Roughness == rhs.Roughness
			&& lhs.MaterialResourceMode == rhs.MaterialResourceMode;
		if (!commonStateMatches)
			return false;

		return lhs.Albedo.x == rhs.Albedo.x
			&& lhs.Albedo.y == rhs.Albedo.y
			&& lhs.Albedo.z == rhs.Albedo.z
			&& lhs.Albedo.w == rhs.Albedo.w
			&& lhs.Material == rhs.Material
			&& lhs.CastShadows == rhs.CastShadows
			&& lhs.ReceiveShadows == rhs.ReceiveShadows;
	}
}
