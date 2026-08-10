#include "Graphics/Instancing/RenderChunk.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <algorithm>
#include <cmath>

namespace enignE::Graphics
{
	RenderChunk::RenderChunk(const ChunkCoord& coord, float chunkSize)
		: m_coord(coord)
		, m_chunkSize(chunkSize)
	{
		const float half = chunkSize * 0.5f;
		m_center = {
			static_cast<float>(coord.X) * chunkSize + half,
			static_cast<float>(coord.Y) * chunkSize + half,
			static_cast<float>(coord.Z) * chunkSize + half
		};
		m_boundsMin = {m_center.x - half, m_center.y - half, m_center.z - half};
		m_boundsMax = {m_center.x + half, m_center.y + half, m_center.z + half};
		m_halfExtents = {half, half, half};
	}

	float RenderChunk::GetHalfExtent() const
	{
		return std::max({m_halfExtents.x, m_halfExtents.y, m_halfExtents.z});
	}

	void RenderChunk::AddEntity(
		entt::entity entity,
		const DirectX::XMFLOAT3& boundsMin,
		const DirectX::XMFLOAT3& boundsMax)
	{
		if (m_entities.empty())
		{
			m_boundsMin = boundsMin;
			m_boundsMax = boundsMax;
		}
		else
		{
			m_boundsMin.x = std::min(m_boundsMin.x, boundsMin.x);
			m_boundsMin.y = std::min(m_boundsMin.y, boundsMin.y);
			m_boundsMin.z = std::min(m_boundsMin.z, boundsMin.z);
			m_boundsMax.x = std::max(m_boundsMax.x, boundsMax.x);
			m_boundsMax.y = std::max(m_boundsMax.y, boundsMax.y);
			m_boundsMax.z = std::max(m_boundsMax.z, boundsMax.z);
		}

		m_center = {
			(m_boundsMin.x + m_boundsMax.x) * 0.5f,
			(m_boundsMin.y + m_boundsMax.y) * 0.5f,
			(m_boundsMin.z + m_boundsMax.z) * 0.5f
		};
		m_halfExtents = {
			(m_boundsMax.x - m_boundsMin.x) * 0.5f,
			(m_boundsMax.y - m_boundsMin.y) * 0.5f,
			(m_boundsMax.z - m_boundsMin.z) * 0.5f
		};

		m_entities.push_back(entity);
	}

	void RenderChunk::ClearEntities()
	{
		m_entities.clear();
		const float half = m_chunkSize * 0.5f;
		m_center = {
			static_cast<float>(m_coord.X) * m_chunkSize + half,
			static_cast<float>(m_coord.Y) * m_chunkSize + half,
			static_cast<float>(m_coord.Z) * m_chunkSize + half
		};
		m_boundsMin = {m_center.x - half, m_center.y - half, m_center.z - half};
		m_boundsMax = {m_center.x + half, m_center.y + half, m_center.z + half};
		m_halfExtents = {half, half, half};
	}

	bool RenderChunk::IsVisibleByDistance(const DirectX::XMFLOAT3& cameraPosition, float maxDistance) const
	{
		const float dx = m_center.x - cameraPosition.x;
		const float dy = m_center.y - cameraPosition.y;
		const float dz = m_center.z - cameraPosition.z;
		const float radius = std::sqrt(
			m_halfExtents.x * m_halfExtents.x
			+ m_halfExtents.y * m_halfExtents.y
			+ m_halfExtents.z * m_halfExtents.z);
		const float visibleDistance = maxDistance + radius;
		return dx * dx + dy * dy + dz * dz <= visibleDistance * visibleDistance;
	}

	bool RenderChunk::IntersectsFrustum(const Frustum& frustum) const
	{
		for (const DirectX::XMFLOAT4& plane : frustum.Planes)
		{
			const float projectedRadius =
				m_halfExtents.x * std::abs(plane.x)
				+ m_halfExtents.y * std::abs(plane.y)
				+ m_halfExtents.z * std::abs(plane.z);

			const float signedDistance =
				plane.x * m_center.x
				+ plane.y * m_center.y
				+ plane.z * m_center.z
				+ plane.w;

			if (signedDistance + projectedRadius < 0.0f)
				return false;
		}

		return true;
	}
}
