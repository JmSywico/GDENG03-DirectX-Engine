#pragma once

#include <DirectXMath.h>
#include <entt/entity/entity.hpp>

#include <cstdint>
#include <vector>

namespace jnpf::Graphics
{
	struct Frustum
	{
		DirectX::XMFLOAT4 Planes[6] = {};
	};

	struct ChunkCoord
	{
		int32_t X = 0;
		int32_t Y = 0;
		int32_t Z = 0;

		bool operator==(const ChunkCoord& other) const
		{
			return X == other.X && Y == other.Y && Z == other.Z;
		}
	};

	struct ChunkCoordHash
	{
		size_t operator()(const ChunkCoord& coord) const
		{
			const size_t x = static_cast<size_t>(static_cast<uint32_t>(coord.X) * 73856093u);
			const size_t y = static_cast<size_t>(static_cast<uint32_t>(coord.Y) * 19349663u);
			const size_t z = static_cast<size_t>(static_cast<uint32_t>(coord.Z) * 83492791u);
			return x ^ y ^ z;
		}
	};

	class RenderChunk
	{
	public:
		RenderChunk() = default;
		RenderChunk(const ChunkCoord& coord, float chunkSize);

		const ChunkCoord& GetCoord() const { return m_coord; }
		const DirectX::XMFLOAT3& GetCenter() const { return m_center; }
		const DirectX::XMFLOAT3& GetBoundsMin() const { return m_boundsMin; }
		const DirectX::XMFLOAT3& GetBoundsMax() const { return m_boundsMax; }
		float GetHalfExtent() const;
		const std::vector<entt::entity>& GetEntities() const { return m_entities; }

		void AddEntity(
			entt::entity entity,
			const DirectX::XMFLOAT3& boundsMin,
			const DirectX::XMFLOAT3& boundsMax);
		void ClearEntities();
		bool IsEmpty() const { return m_entities.empty(); }
		bool IsVisibleByDistance(const DirectX::XMFLOAT3& cameraPosition, float maxDistance) const;
		bool IntersectsFrustum(const Frustum& frustum) const;

	private:
		ChunkCoord m_coord;
		DirectX::XMFLOAT3 m_center = {0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 m_boundsMin = {0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 m_boundsMax = {0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 m_halfExtents = {0.0f, 0.0f, 0.0f};
		float m_chunkSize = 32.0f;
		std::vector<entt::entity> m_entities;
	};
}
