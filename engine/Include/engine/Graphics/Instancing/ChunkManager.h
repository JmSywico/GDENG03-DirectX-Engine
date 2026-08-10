#pragma once

#include "RenderChunk.h"

#include <DirectXMath.h>
#include <entt/entity/registry.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

class Model;
struct MaterialResource;
namespace jnpf::Core { class JobSystem; }

namespace jnpf::Graphics
{
	class ChunkManager
	{
	public:
		explicit ChunkManager(float chunkSize = 32.0f);

		void SetChunkSize(float chunkSize);
		void Clear();
		float GetChunkSize() const { return m_chunkSize; }

		ChunkCoord WorldToChunkCoord(const DirectX::XMFLOAT3& worldPosition) const;
		std::vector<ChunkCoord> UpdateFromRegistry(
			entt::registry& registry,
			Core::JobSystem* jobs = nullptr);
		std::vector<const RenderChunk*> QueryVisibleChunks(
			const DirectX::XMFLOAT3& cameraPosition,
			float maxDistance,
			const Frustum& frustum) const;
		const std::unordered_map<ChunkCoord, RenderChunk, ChunkCoordHash>& GetChunks() const { return m_chunks; }

	private:
		void AddEntityToChunk(
			entt::entity entity,
			const DirectX::XMFLOAT3& worldPosition,
			const DirectX::XMFLOAT3& boundsMin,
			const DirectX::XMFLOAT3& boundsMax);

		struct EntityRenderState
		{
			ChunkCoord Coord;
			DirectX::XMFLOAT3 BoundsMin = {0.0f, 0.0f, 0.0f};
			DirectX::XMFLOAT3 BoundsMax = {0.0f, 0.0f, 0.0f};
			DirectX::XMFLOAT4X4 World = {};
			const Model* ModelPtr = nullptr;
			const MaterialResource* MaterialResourcePtr = nullptr;
			const void* MaterialTexturePtr = nullptr;
			const void* NormalTexturePtr = nullptr;
			const void* MetallicRoughnessTexturePtr = nullptr;
			DirectX::XMFLOAT4 MaterialAlbedo = {1.0f, 1.0f, 1.0f, 1.0f};
			DirectX::XMFLOAT3 MaterialEmissive = {0.0f, 0.0f, 0.0f};
			float Metallic = 0.0f;
			float Roughness = 1.0f;
			std::uint8_t MaterialResourceMode = 0;
			DirectX::XMFLOAT4 Albedo = {1.0f, 1.0f, 1.0f, 1.0f};
			std::uint8_t Material = 0;
			bool CastShadows = true;
			bool ReceiveShadows = true;
		};

		static bool RenderStateMatches(const EntityRenderState& lhs, const EntityRenderState& rhs);

		float m_chunkSize = 32.0f;
		std::unordered_map<ChunkCoord, RenderChunk, ChunkCoordHash> m_chunks;
		std::unordered_map<entt::entity, EntityRenderState> m_entityStates;
	};
}
